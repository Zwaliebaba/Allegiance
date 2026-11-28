#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// Key framed transformations
//
//////////////////////////////////////////////////////////////////////////////

class KeyFramedTranslateTransformImpl : public KeyFramedTranslateTransform {
private:
    class KeyData {
    public:
        float  m_frame;
        Vector m_vec;
    };

    std::vector<KeyData> m_keys;

    Number* GetFrame() { return Number::Cast(GetChild(0)); }

public:
    KeyFramedTranslateTransformImpl(Number* pframe) :
        KeyFramedTranslateTransform(pframe)
    {
    }

    void AddKey(float frame, const Vector& vec) override
    {
        int count = (int)m_keys.size();

        ZAssert(count == 0 || frame > m_keys.back().m_frame);

        m_keys.emplace_back();
        m_keys.back().m_frame = frame;
        m_keys.back().m_vec   = vec;
    }

    //
    // Value members
    //

    /*  , no animations
    bool IsConstant()
    {
        int count = m_keys.GetCount();

        if (count > 0) {
            for (int index = 1; index < count; index++) {
                if (m_keys[0].m_vec != m_keys[index].m_vec) {
                    return false;
                }
            }
        }

        return true;
    }
    */

    void Evaluate() override
    {
        float frame = GetFrame()->GetValue();

        int count = (int)m_keys.size();

        if (frame <= m_keys[0].m_frame) {
            //
            // Before first key
            //

            GetValueInternal().SetTranslate(m_keys[0].m_vec);
        } else if (frame >= m_keys[count - 1].m_frame) {
            //
            // After last key
            //

            GetValueInternal().SetTranslate(m_keys[count - 1].m_vec);
        } else {
            //
            // Between two keys
            //

            int index = 0;

            while (frame > m_keys[index + 1].m_frame) {
                index++;
            }

            Vector vecDelta    = m_keys[index + 1].m_vec - m_keys[index].m_vec;
            float  interpolant = (frame - m_keys[index].m_frame) / (m_keys[index + 1].m_frame - m_keys[index].m_frame);

            GetValueInternal().SetTranslate(m_keys[index].m_vec + vecDelta * interpolant);
        }
    }

    ZString GetString(int indent) override
    {
        // KeyFramedTranslate([ time, x, y, z ], frame)

        ZString str = "KeyFramedTranslate(\n" + Indent(indent + 1) + "[\n";

        int count = (int)m_keys.size();
        for (int index = 0; index < count; index++) {
            str +=
                  Indent(indent + 2)
                + StrFromFloat(m_keys[index].m_frame) + ", "
                + StrFromFloat(m_keys[index].m_vec.X()) + ", "
                + StrFromFloat(m_keys[index].m_vec.Y()) + ", "
                + StrFromFloat(m_keys[index].m_vec.Z());

            if (index < count - 1) {
                str += ",\n";
            } else {
                str += "\n";
            }
        }

        return
              str
            + Indent(indent + 1) + "],\n"
            + Indent(indent + 1) + GetFrame()->GetChildString(indent + 1) + "\n"
            + Indent(indent) + ")";
    }
};

TRef<KeyFramedTranslateTransform> CreateKeyFramedTranslateTransform(Number* pframe)
{
    return new KeyFramedTranslateTransformImpl(pframe);
}

class KeyFramedScaleTransformImpl : public KeyFramedScaleTransform {
private:
    class KeyData {
    public:
        float  m_frame;
        Vector m_vec;
    };

    std::vector<KeyData> m_keys;

    Number* GetFrame() { return Number::Cast(GetChild(0)); }

public:
    KeyFramedScaleTransformImpl(Number* pframe) :
        KeyFramedScaleTransform(pframe)
    {
    }

    void AddKey(float frame, const Vector& vec) override
    {
        int count = (int)m_keys.size();

        ZAssert(count == 0 || frame > m_keys.back().m_frame);

        m_keys.emplace_back();
        m_keys.back().m_frame = frame;
        m_keys.back().m_vec   = vec;
    }

    /*  , no animations
    bool IsConstant()
    {
        int count = m_keys.GetCount();

        if (count > 0) {
            for (int index = 1; index < count; index++) {
                if (m_keys[0].m_vec != m_keys[index].m_vec) {
                    return false;
                }
            }
        }

        return true;
    }
    */

    void Evaluate() override
    {
        float frame = GetFrame()->GetValue();

        int count = (int)m_keys.size();

        if (frame <= m_keys[0].m_frame) {
            //
            // Before first key
            //

            GetValueInternal().SetScale(m_keys[0].m_vec);
        } else if (frame >= m_keys[count - 1].m_frame) {
            //
            // After last key
            //

            GetValueInternal().SetScale(m_keys[count - 1].m_vec);
        } else {
            //
            // Between two keys
            //

            int index = 0;

            while (frame > m_keys[index + 1].m_frame) {
                index++;
            }

            Vector vecDelta    = m_keys[index + 1].m_vec - m_keys[index].m_vec;
            float  interpolant = (frame - m_keys[index].m_frame) / (m_keys[index + 1].m_frame - m_keys[index].m_frame);

            GetValueInternal().SetScale(m_keys[index].m_vec + vecDelta * interpolant);
        }
    }

    ZString GetString(int indent) override
    {
        // KeyFramedScale([ time, x, y, z ], frame)

        ZString str = "KeyFramedScale(\n" + Indent(indent + 1) + "[\n";

        int count = (int)m_keys.size();
        for (int index = 0; index < count; index++) {
            str +=
                  Indent(indent + 2)
                + StrFromFloat(m_keys[index].m_frame) + ", "
                + StrFromFloat(m_keys[index].m_vec.X()) + ", "
                + StrFromFloat(m_keys[index].m_vec.Y()) + ", "
                + StrFromFloat(m_keys[index].m_vec.Z());

            if (index < count - 1) {
                str += ",\n";
            } else {
                str += "\n";
            }
        }

        return
              str
            + Indent(indent + 1) + "],\n"
            + Indent(indent + 1) + GetFrame()->GetChildString(indent + 1) + "\n"
            + Indent(indent) + ")";
    }
};

TRef<KeyFramedScaleTransform> CreateKeyFramedScaleTransform(Number* pframe)
{
    return new KeyFramedScaleTransformImpl(pframe);
}

class KeyFramedRotateTransformImpl : public KeyFramedRotateTransform {
private:
    class KeyData {
    public:
        float      m_frame;
        Quaternion m_quat;
    };

    std::vector<KeyData> m_keys;

    Number* GetFrame() { return Number::Cast(GetChild(0)); }

public:
    KeyFramedRotateTransformImpl(Number* pframe) :
        KeyFramedRotateTransform(pframe)
    {
    }

    void AddKey(float frame, const Quaternion& quat) override
    {
        int count = (int)m_keys.size();

        ZAssert(count == 0 || frame > m_keys.back().m_frame);

        m_keys.emplace_back();
        m_keys.back().m_frame = frame;
        m_keys.back().m_quat  = quat;
    }

    /*  , no animations
    bool IsConstant()
    {
        int count = m_keys.GetCount();

        if (count > 0) {
            for (int index = 1; index < count; index++) {
                if (m_keys[0].m_quat != m_keys[index].m_quat) {
                    return false;
                }
            }
        }

        return true;
    }
    */

    void Evaluate() override
    {
        float frame = GetFrame()->GetValue();

        int count = (int)m_keys.size();

        if (frame <= m_keys[0].m_frame) {
            //
            // Before first key
            //

            //  , add a SetRotate(const Quaternion&) to Matrix

            Vector vec;
            float angle = m_keys[0].m_quat.GetRotation(vec);
            GetValueInternal().SetRotate(vec, angle);
        } else if (frame >= m_keys[count - 1].m_frame) {
            //
            // After last key
            //

            Vector vec;
            float angle = m_keys[count - 1].m_quat.GetRotation(vec);
            GetValueInternal().SetRotate(vec, angle);
        } else {
            //
            // Between two keys
            //

            int index = 0;

            while (frame > m_keys[index + 1].m_frame) {
                index++;
            }

            float interpolant = (frame - m_keys[index].m_frame) / (m_keys[index + 1].m_frame - m_keys[index].m_frame);

            Quaternion quat = Slerp(m_keys[index].m_quat, m_keys[index + 1].m_quat, interpolant);
            Vector vec;
            float angle = quat.GetRotation(vec);
            GetValueInternal().SetRotate(vec, angle);
        }
    }

    ZString GetString(int indent) override
    {
        // KeyFramedRotate([ time, x, y, z ], frame)

        ZString str = "KeyFramedRotate(\n" + Indent(indent + 1) + "[\n";

        int count = (int)m_keys.size();
        for (int index = 0; index < count; index++) {
            str +=
                  Indent(indent + 2)
                + StrFromFloat(m_keys[index].m_frame) + ", "
                + StrFromFloat(m_keys[index].m_quat.GetA()) + ", "
                + StrFromFloat(m_keys[index].m_quat.GetX()) + ", "
                + StrFromFloat(m_keys[index].m_quat.GetY()) + ", "
                + StrFromFloat(m_keys[index].m_quat.GetZ());

            if (index < count - 1) {
                str += ",\n";
            } else {
                str += "\n";
            }
        }

        return
              str
            + Indent(indent + 1) + "],\n"
            + Indent(indent + 1) + GetFrame()->GetChildString(indent + 1) + "\n"
            + Indent(indent) + ")";
    }
};

TRef<KeyFramedRotateTransform> CreateKeyFramedRotateTransform(Number* pframe)
{
    return new KeyFramedRotateTransformImpl(pframe);
}
