//////////////////////////////////////////////////////////////////////////////
//
// xmunge: prepares DirectX .x geometry for the collision-hull tool (cvh).
//
//   xmunge -flatten <frame> <in.x> <out.x>   all meshes of the frame hierarchy,
//                                            posed at <frame>, merged into one
//                                            mesh in model space
//   xmunge -extract <frame> <in.x> <out>     attachment points (weapon emitters,
//                                            cockpit, launch bays, garages)
//                                            -> <out>.dat
//   xmunge -bound   <frame> <in.x> <out>     each second-level frame named
//                                            "*bound*" as its own mesh
//                                            -> <out>_<n>.mesh
//
// The original was built on Direct3D Retained Mode, which Windows no longer
// ships. This version reimplements the same operations on a small text .x
// reader, matching D3DRM's behaviour:
//
//  - Only the first top-level Frame is loaded (D3DRMLOAD_FIRST). A file without
//    frames contributes its first Mesh unchanged.
//  - Frame matrices use the .x row-vector convention (p' = p * M); a child's
//    world matrix is local * parent.
//  - An animated frame's matrix is replaced by scale * rotation * translation
//    evaluated from its keys at <frame>. Missing key types are identity;
//    positions and scales interpolate linearly, rotations by slerp, and times
//    outside the keys clamp to the first/last key.
//  - Output uses the layout D3DRM's mesh builder wrote, which cvh parses line
//    by line ("Mesh {", the vertex count, then one "x;y;z;," per line).
//
// The original -reduce (progressive-mesh LOD reduction) is not carried over:
// reduced LOD meshes were dropped from the art pipeline (see MODERNIZATION.md).
//
//////////////////////////////////////////////////////////////////////////////

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

//////////////////////////////////////////////////////////////////////////////
// .x text parser (the subset this art uses: no binary or compressed files)
//////////////////////////////////////////////////////////////////////////////

struct Node {
    std::string                         type;
    std::string                         name;
    std::vector<double>                 numbers;
    std::vector<std::string>            refs;      // { Name } references
    std::vector<std::unique_ptr<Node>>  kids;
};

class Parser {
public:
    explicit Parser(const std::string& text) : m_s(text), m_i(0) {}

    bool Parse(Node& root, std::string& error)
    {
        if (m_s.compare(0, 4, "xof ") != 0 || m_s.size() < 16) {
            error = "not a DirectX .x file";
            return false;
        }
        if (m_s.compare(8, 3, "txt") != 0) {
            error = "only text .x files are supported";
            return false;
        }
        m_i = 16;
        return Block(root, false, error);
    }

private:
    enum Kind { End, Ident, Number, String, Guid, Punct };

    struct Token { Kind kind; std::string text; };

    Token Next()
    {
        for (;;) {
            while (m_i < m_s.size() && isspace((unsigned char)m_s[m_i]))
                ++m_i;
            if (m_i >= m_s.size())
                return { End, "" };
            if (m_s[m_i] == '#' || (m_s[m_i] == '/' && m_i + 1 < m_s.size() && m_s[m_i + 1] == '/')) {
                while (m_i < m_s.size() && m_s[m_i] != '\n')
                    ++m_i;
                continue;
            }
            break;
        }

        size_t start = m_i;
        char c = m_s[m_i];

        if (c == '"') {
            ++m_i;
            while (m_i < m_s.size() && m_s[m_i] != '"')
                ++m_i;
            ++m_i;
            return { String, m_s.substr(start, m_i - start) };
        }
        if (c == '<') {
            while (m_i < m_s.size() && m_s[m_i] != '>')
                ++m_i;
            ++m_i;
            return { Guid, m_s.substr(start, m_i - start) };
        }
        if (c == '.' && m_s.compare(m_i, 3, "...") == 0) {
            m_i += 3;
            return { Punct, "..." };
        }
        if (strchr("{};,[]", c)) {
            ++m_i;
            return { Punct, std::string(1, c) };
        }
        if (isdigit((unsigned char)c) || c == '-' || c == '+' || c == '.') {
            ++m_i;
            while (m_i < m_s.size() && (isdigit((unsigned char)m_s[m_i]) || strchr(".eE+-", m_s[m_i])))
                ++m_i;
            return { Number, m_s.substr(start, m_i - start) };
        }
        if (isalpha((unsigned char)c) || c == '_') {
            ++m_i;
            while (m_i < m_s.size() && (isalnum((unsigned char)m_s[m_i]) || strchr("_.-", m_s[m_i])))
                ++m_i;
            return { Ident, m_s.substr(start, m_i - start) };
        }
        ++m_i;
        return { Punct, std::string(1, c) };
    }

    static void Warn(const std::string& what)
    {
        fprintf(stderr, "xmunge: warning: %s\n", what.c_str());
    }

    Token Peek()
    {
        size_t save = m_i;
        Token t = Next();
        m_i = save;
        return t;
    }

    void SkipBraces()
    {
        int depth = 0;
        for (Token t = Next(); t.kind != End; t = Next()) {
            if (t.kind == Punct && t.text == "{")
                ++depth;
            else if (t.kind == Punct && t.text == "}" && --depth == 0)
                return;
        }
    }

    bool Block(Node& node, bool closed, std::string& error)
    {
        for (;;) {
            Token t = Next();

            // Some hand-edited art has unbalanced braces. D3DRM accepted it:
            // a stray '}' ends the file, and end of file closes open objects.
            if (t.kind == End) {
                if (closed)
                    Warn("end of file inside " + node.type);
                return true;
            }
            if (t.kind == Punct && t.text == "}") {
                if (!closed) {
                    Warn("unbalanced '}'");
                    m_i = m_s.size();
                }
                return true;
            }
            if (t.kind == Number) {
                node.numbers.push_back(strtod(t.text.c_str(), nullptr));
                continue;
            }
            if (t.kind == Ident && t.text == "template") {
                SkipBraces();
                continue;
            }
            if (t.kind == Punct && t.text == "{") {
                // A reference to a named object: { Name }
                std::string ref;
                for (Token r = Next(); !(r.kind == Punct && r.text == "}"); r = Next()) {
                    if (r.kind == End) {
                        error = "unexpected end of file in reference";
                        return false;
                    }
                    if (!ref.empty())
                        ref += ' ';
                    ref += r.text;
                }
                node.refs.push_back(ref);
                continue;
            }
            if (t.kind == Ident) {
                // Data object: Type [Name] { ... }
                Token n = Peek();
                std::string name;
                if (n.kind == Ident) {
                    name = Next().text;
                    n = Peek();
                }
                if (n.kind == Punct && n.text == "{") {
                    Next();
                    auto child = std::make_unique<Node>();
                    child->type = t.text;
                    child->name = name;
                    if (!Block(*child, true, error))
                        return false;
                    node.kids.push_back(std::move(child));
                }
                continue;
            }
            // separators, strings and GUIDs carry nothing this tool needs
        }
    }

    const std::string& m_s;
    size_t             m_i;
};

//////////////////////////////////////////////////////////////////////////////
// Math: row-vector convention, p' = p * M
//////////////////////////////////////////////////////////////////////////////

struct Matrix {
    double m[4][4];
    static Matrix Identity()
    {
        Matrix r = {};
        for (int i = 0; i < 4; ++i)
            r.m[i][i] = 1;
        return r;
    }
};

Matrix operator*(const Matrix& a, const Matrix& b)
{
    Matrix r = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}

struct Vec3 { double x, y, z; };

Vec3 Transform(const Vec3& p, const Matrix& m)
{
    return {
        p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
        p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
        p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
    };
}

Matrix Rotation(double w, double x, double y, double z)
{
    Matrix r = Matrix::Identity();
    r.m[0][0] = 1 - 2 * (y * y + z * z); r.m[0][1] = 2 * (x * y + w * z);     r.m[0][2] = 2 * (x * z - w * y);
    r.m[1][0] = 2 * (x * y - w * z);     r.m[1][1] = 1 - 2 * (x * x + z * z); r.m[1][2] = 2 * (y * z + w * x);
    r.m[2][0] = 2 * (x * z + w * y);     r.m[2][1] = 2 * (y * z - w * x);     r.m[2][2] = 1 - 2 * (x * x + y * y);
    return r;
}

Matrix Scale(double x, double y, double z)
{
    Matrix r = Matrix::Identity();
    r.m[0][0] = x; r.m[1][1] = y; r.m[2][2] = z;
    return r;
}

Matrix Translation(double x, double y, double z)
{
    Matrix r = Matrix::Identity();
    r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z;
    return r;
}

//////////////////////////////////////////////////////////////////////////////
// Animation
//////////////////////////////////////////////////////////////////////////////

struct Key { double time; std::vector<double> values; };

struct FrameAnimation {
    std::vector<Key> keys[3];   // 0 = rotation (w, x, y, z), 1 = scale, 2 = position
    bool Any() const { return !keys[0].empty() || !keys[1].empty() || !keys[2].empty(); }
};

std::vector<double> Sample(const std::vector<Key>& keys, double t, bool rotation)
{
    if (t <= keys.front().time)
        return keys.front().values;
    if (t >= keys.back().time)
        return keys.back().values;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        const Key& k0 = keys[i];
        const Key& k1 = keys[i + 1];
        if (t < k0.time || t > k1.time)
            continue;

        double u = (k1.time > k0.time) ? (t - k0.time) / (k1.time - k0.time) : 0;
        std::vector<double> a = k0.values;
        std::vector<double> b = k1.values;
        std::vector<double> r(a.size());

        if (!rotation || a.size() != 4) {
            for (size_t j = 0; j < a.size(); ++j)
                r[j] = a[j] + (b[j] - a[j]) * u;
            return r;
        }

        double d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
        if (d < 0) {
            for (double& v : b) v = -v;
            d = -d;
        }
        if (d > 0.9995) {
            for (size_t j = 0; j < 4; ++j)
                r[j] = a[j] + (b[j] - a[j]) * u;
        } else {
            double theta = acos(d > 1 ? 1 : d);
            double s = sin(theta);
            for (size_t j = 0; j < 4; ++j)
                r[j] = (sin((1 - u) * theta) * a[j] + sin(u * theta) * b[j]) / s;
        }
        double len = sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
        for (double& v : r) v /= len;
        return r;
    }
    return keys.back().values;
}

bool ReadAnimationKeys(const Node& key, FrameAnimation& anim)
{
    const std::vector<double>& d = key.numbers;
    if (d.size() < 2)
        return false;
    int type  = (int)d[0];
    int count = (int)d[1];
    size_t i = 2;
    std::vector<Key> keys;
    for (int k = 0; k < count; ++k) {
        if (i + 2 > d.size())
            return false;
        Key kk;
        kk.time = d[i];
        size_t n = (size_t)d[i + 1];
        i += 2;
        if (i + n > d.size())
            return false;
        kk.values.assign(d.begin() + i, d.begin() + i + n);
        i += n;
        keys.push_back(kk);
    }
    if (type < 0 || type > 2) {
        fprintf(stderr, "xmunge: warning: animation key type %d ignored\n", type);
        return true;
    }
    anim.keys[type] = keys;
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// Model
//////////////////////////////////////////////////////////////////////////////

struct MeshData {
    std::vector<Vec3>                 vertices;
    std::vector<std::vector<int>>     faces;
};

bool ReadMesh(const Node& mesh, const Matrix& world, MeshData& out)
{
    const std::vector<double>& d = mesh.numbers;
    if (d.empty())
        return false;
    size_t nVertices = (size_t)d[0];
    size_t i = 1;
    if (i + 3 * nVertices > d.size())
        return false;

    int base = (int)out.vertices.size();
    for (size_t v = 0; v < nVertices; ++v, i += 3)
        out.vertices.push_back(Transform({ d[i], d[i + 1], d[i + 2] }, world));

    if (i < d.size()) {
        size_t nFaces = (size_t)d[i++];
        for (size_t f = 0; f < nFaces; ++f) {
            if (i >= d.size())
                return false;
            size_t n = (size_t)d[i++];
            if (i + n > d.size())
                return false;
            std::vector<int> face;
            for (size_t k = 0; k < n; ++k)
                face.push_back(base + (int)d[i + k]);
            i += n;
            out.faces.push_back(face);
        }
    }
    return true;
}

class Model {
public:
    bool Load(const std::string& path, double time, std::string& error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            error = "cannot open " + path;
            return false;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        m_text = ss.str();

        Parser parser(m_text);
        if (!parser.Parse(m_root, error)) {
            error = path + ": " + error;
            return false;
        }

        m_time = time;

        for (auto& k : m_root.kids) {
            if (k->type == "AnimationSet") {
                for (auto& a : k->kids) {
                    if (a->type != "Animation" || a->refs.empty())
                        continue;
                    FrameAnimation anim;
                    for (auto& key : a->kids) {
                        if (key->type == "AnimationKey" && !ReadAnimationKeys(*key, anim)) {
                            error = path + ": malformed AnimationKey in " + a->name;
                            return false;
                        }
                    }
                    m_animations[a->refs[0]] = anim;
                }
                break;  // D3DRMLOAD_FIRST: the first AnimationSet only
            }
        }
        return true;
    }

    const Node* TopFrame() const
    {
        for (auto& k : m_root.kids)
            if (k->type == "Frame")
                return k.get();
        return nullptr;
    }

    const Node* FirstMesh() const
    {
        for (auto& k : m_root.kids)
            if (k->type == "Mesh")
                return k.get();
        return nullptr;
    }

    Matrix Local(const Node& frame) const
    {
        auto it = m_animations.find(frame.name);
        if (it != m_animations.end() && it->second.Any()) {
            const FrameAnimation& a = it->second;
            Matrix r = Matrix::Identity();
            Matrix s = Matrix::Identity();
            Matrix t = Matrix::Identity();
            if (!a.keys[0].empty()) {
                std::vector<double> q = Sample(a.keys[0], m_time, true);
                if (q.size() == 4) r = Rotation(q[0], q[1], q[2], q[3]);
            }
            if (!a.keys[1].empty()) {
                std::vector<double> v = Sample(a.keys[1], m_time, false);
                if (v.size() == 3) s = Scale(v[0], v[1], v[2]);
            }
            if (!a.keys[2].empty()) {
                std::vector<double> v = Sample(a.keys[2], m_time, false);
                if (v.size() == 3) t = Translation(v[0], v[1], v[2]);
            }
            return s * r * t;
        }

        for (auto& k : frame.kids) {
            if (k->type == "FrameTransformMatrix" && k->numbers.size() >= 16) {
                Matrix m;
                for (int i = 0; i < 16; ++i)
                    m.m[i / 4][i % 4] = k->numbers[i];
                return m;
            }
        }
        return Matrix::Identity();
    }

    // Meshes of frame and its descendants, in the space of parent.
    bool Collect(const Node& frame, const Matrix& parent, MeshData& out) const
    {
        Matrix world = Local(frame) * parent;
        for (auto& k : frame.kids) {
            if (k->type == "Mesh") {
                if (!ReadMesh(*k, world, out))
                    return false;
            } else if (k->type == "Frame") {
                if (!Collect(*k, world, out))
                    return false;
            }
        }
        return true;
    }

    // Every frame under frame (inclusive) with its world matrix, in document order.
    template <class F>
    void Walk(const Node& frame, const Matrix& parent, F&& visit) const
    {
        Matrix world = Local(frame) * parent;
        visit(frame, world);
        for (auto& k : frame.kids)
            if (k->type == "Frame")
                Walk(*k, world, visit);
    }

private:
    std::string                           m_text;
    Node                                  m_root;
    double                                m_time = 0;
    std::map<std::string, FrameAnimation> m_animations;
};

//////////////////////////////////////////////////////////////////////////////
// Output
//////////////////////////////////////////////////////////////////////////////

bool WriteMesh(const std::string& path, const MeshData& mesh)
{
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        fprintf(stderr, "xmunge: cannot write %s\n", path.c_str());
        return false;
    }

    fprintf(f, "xof 0302txt 0032\n\nHeader {\n 1;\n 0;\n 1;\n}\n\n");
    fprintf(f, "Mesh {\n %d;\n", (int)mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Vec3& v = mesh.vertices[i];
        fprintf(f, " %f;%f;%f;%s\n", v.x, v.y, v.z, (i + 1 == mesh.vertices.size()) ? ";" : ",");
    }
    fprintf(f, "\n %d;\n", (int)mesh.faces.size());
    for (size_t i = 0; i < mesh.faces.size(); ++i) {
        const std::vector<int>& face = mesh.faces[i];
        fprintf(f, " %d;", (int)face.size());
        for (size_t k = 0; k < face.size(); ++k)
            fprintf(f, "%d%s", face[k], (k + 1 == face.size()) ? ";" : ",");
        fprintf(f, "%s\n", (i + 1 == mesh.faces.size()) ? ";" : ",");
    }
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

bool Contains(const std::string& s, const char* token)
{
    return s.find(token) != std::string::npos;
}

//////////////////////////////////////////////////////////////////////////////
// Operations
//////////////////////////////////////////////////////////////////////////////

int Flatten(const Model& model, const std::string& out)
{
    MeshData mesh;
    if (const Node* top = model.TopFrame()) {
        if (!model.Collect(*top, Matrix::Identity(), mesh)) {
            fprintf(stderr, "xmunge: malformed mesh data\n");
            return 1;
        }
    } else if (const Node* m = model.FirstMesh()) {
        if (!ReadMesh(*m, Matrix::Identity(), mesh)) {
            fprintf(stderr, "xmunge: malformed mesh data\n");
            return 1;
        }
    } else {
        fprintf(stderr, "xmunge: no frame or mesh in input\n");
        return 1;
    }
    return WriteMesh(out, mesh) ? 0 : 1;
}

int Extract(const Model& model, const std::string& out)
{
    const Node* top = model.TopFrame();
    if (!top)
        return 0;   // like the original: no frames, no .dat file

    std::string path = out + ".dat";
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        fprintf(stderr, "xmunge: cannot write %s\n", path.c_str());
        return 1;
    }

    static const char* const tokens[] = {
        "wepemt", "turwepemt", "missemt", "cockpt", "launch", "garage", "capgarage"
    };

    model.Walk(*top, Matrix::Identity(), [&](const Node& frame, const Matrix& world) {
        bool match = false;
        for (const char* t : tokens)
            match = match || Contains(frame.name, t);
        if (!match)
            return;

        // The original printed the frame name without its first four
        // characters (the exporter's "frm-" prefix).
        const char* name = frame.name.size() > 4 ? frame.name.c_str() + 4 : "";

        double dx = world.m[2][0], dy = world.m[2][1], dz = world.m[2][2];
        double len = sqrt(dx * dx + dy * dy + dz * dz);
        if (len > 0) { dx /= len; dy /= len; dz /= len; }

        fputs(name, f);
        fprintf(f, "\n%20.10f%20.10f%20.10f\n", world.m[3][0], world.m[3][1], world.m[3][2]);
        fprintf(f, "%20.10f%20.10f%20.10f\n", dx, dy, dz);
    });

    fclose(f);
    return 0;
}

int Bound(const Model& model, const std::string& out)
{
    const Node* top = model.TopFrame();
    if (!top)
        return 0;

    // Re-parenting in D3DRM preserved world position (checked against the art:
    // bounding pieces only line up with the model in model space), so each
    // piece is written in model space.
    Matrix topWorld = model.Local(*top);
    int id = 0;
    for (auto& child : top->kids) {
        if (child->type != "Frame" || !Contains(child->name, "bound"))
            continue;
        MeshData mesh;
        if (!model.Collect(*child, topWorld, mesh)) {
            fprintf(stderr, "xmunge: malformed mesh data in %s\n", child->name.c_str());
            return 1;
        }
        if (!WriteMesh(out + "_" + std::to_string(id++) + ".mesh", mesh))
            return 1;
    }
    return 0;
}

int Usage()
{
    fprintf(stderr,
        "Usage: xmunge -flatten <frame> <in.x> <out.x>\n"
        "       xmunge -extract <frame> <in.x> <out>      (writes <out>.dat)\n"
        "       xmunge -bound   <frame> <in.x> <out>      (writes <out>_<n>.mesh)\n");
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    std::string mode;
    double frame = 0;
    std::string in, out;

    if (argc == 5 && argv[1][0] == '-') {
        mode  = argv[1] + 1;
        frame = strtod(argv[2], nullptr);
        in    = argv[3];
        out   = argv[4];
    } else if (argc == 3) {
        // Like the original, no mode means -flatten at time 0.
        mode = "flatten";
        in   = argv[1];
        out  = argv[2];
    } else {
        return Usage();
    }

    if (mode == "reduce") {
        fprintf(stderr, "xmunge: -reduce was removed with the reduced LOD meshes (see MODERNIZATION.md)\n");
        return 2;
    }
    if (mode != "flatten" && mode != "extract" && mode != "bound")
        return Usage();

    Model model;
    std::string error;
    if (!model.Load(in, frame, error)) {
        fprintf(stderr, "xmunge: %s\n", error.c_str());
        return 1;
    }

    if (mode == "flatten") { printf("Flattening %s\n", in.c_str()); return Flatten(model, out); }
    if (mode == "extract") { printf("Extracting %s\n", in.c_str()); return Extract(model, out); }
    printf("Bounding %s\n", in.c_str());
    return Bound(model, out);
}
