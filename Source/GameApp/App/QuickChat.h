class SonicChatVoice : public IMDLObject {
public:
    struct Data {
        TRef< IObject > m_sound;
        ZString m_string;
    };

    Data& GetData() { return *(Data*)GetDataPointer(); }

    TRef< IObject >& GetSound() { return GetData().m_sound; }
    ZString& GetString() { return GetData().m_string; }
};

class SonicChat : public IMDLObject {
public:
    struct Data {
        TRef<IObjectList> m_voices;
    };

    Data& GetData() { return *(Data*)GetDataPointer(); }

    TRef<IObjectList>& GetVoices() { return GetData().m_voices; }
};

class QuickChatCommand : public QuickChatNode {
public:
    struct Data {
        TRef< SonicChat > m_sonicChat;
        float m_commandID;
        float m_targetType;
        float m_abilityMask;
    };

    Data& GetData() { return *(Data*)GetDataPointer(); }

    TRef< SonicChat >& GetSonicChat() { return GetData().m_sonicChat; }
    float& GetCommandID() { return GetData().m_commandID; }
    float& GetTargetType() { return GetData().m_targetType; }
    float& GetAbilityMask() { return GetData().m_abilityMask; }
};

class QuickChatMenuItem : public IMDLObject {
public:
    struct Data {
        float m_char;
        ZString m_string;
        TRef< QuickChatNode > m_node;
    };

    Data& GetData() { return *(Data*)GetDataPointer(); }

    float& GetChar() { return GetData().m_char; }
    ZString& GetString() { return GetData().m_string; }
    TRef< QuickChatNode >& GetNode() { return GetData().m_node; }
};

class QuickChatMenu : public QuickChatNode {
public:
    struct Data {
        TRef<IObjectList> m_items;
    };

    Data& GetData() { return *(Data*)GetDataPointer(); }

    TRef<IObjectList>& GetItems() { return GetData().m_items; }
};
