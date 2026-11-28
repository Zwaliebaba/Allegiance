#include "pch.h"
#include "serverevents.h"

//////////////////////////////////////////////////////////////////////////////
//
// The server's event table (Server/serverevents_defs.h), which is edited by
// hand now that AGC's XML is gone: every event defined once, with a name and
// a format the log can expand.
//
//////////////////////////////////////////////////////////////////////////////

namespace ServerEventTests
{
    static const ServerEventDef s_defs[] = { SERVER_EVENT_DEFS };

    TEST_CLASS(EventTable)
    {
    public:
        TEST_METHOD(IdsAndNamesAreUnique)
        {
            std::set<long>        ids;
            std::set<std::string> names;
            for (const ServerEventDef& def : s_defs)
            {
                Assert::IsTrue(ids.insert(def.m_id).second, Wide("event " + std::to_string(def.m_id) + " is defined twice").c_str());
                Assert::IsNotNull(def.m_szName);
                Assert::IsTrue(names.insert(def.m_szName).second, Wide(std::string("event name ") + def.m_szName + " is used twice").c_str());
            }
        }

        TEST_METHOD(ServerEventsAreDefined)
        {
            // The ones the server raises on every run
            const long rgid[] = { AllsrvEventID_Initialize, AllsrvEventID_Terminate, AllsrvEventID_GameStarted,
                                  AllsrvEventID_GameEnded, EventID_LoginServer, EventID_LogoutServer };
            for (long id : rgid)
            {
                bool fFound = false;
                for (const ServerEventDef& def : s_defs)
                    fFound |= def.m_id == id;
                Assert::IsTrue(fFound, Wide("event " + std::to_string(id) + " has no definition").c_str());
            }
        }

        TEST_METHOD(FormatsAreWellFormed)
        {
            // %Name%, %.Field% (one of the event's own) or %% (a percent sign)
            const std::set<std::string> fields = { "SubjectName", "SubjectID", "ComputerName", "Context", "ID", "Time" };

            for (const ServerEventDef& def : s_defs)
            {
                Assert::IsNotNull(def.m_szFormat);
                std::wstring where = Wide(std::string(def.m_szName) + ": " + def.m_szFormat);

                for (const char* pch = def.m_szFormat; *pch; pch++)
                {
                    if (*pch != '%')
                        continue;
                    const char* pchEnd = strchr(pch + 1, '%');
                    Assert::IsNotNull(pchEnd, (L"unmatched %: " + where).c_str());

                    std::string name(pch + 1, pchEnd);
                    if (!name.empty() && name[0] == '.')
                        Assert::IsTrue(fields.count(name.substr(1)) == 1, (L"unknown field: " + where).c_str());
                    pch = pchEnd;
                }
            }
        }

        TEST_METHOD(SeveritiesAreKnown)
        {
            for (const ServerEventDef& def : s_defs)
                Assert::IsTrue(def.m_severity >= EventSeverity_Info && def.m_severity <= EventSeverity_Error);
        }
    };
}
