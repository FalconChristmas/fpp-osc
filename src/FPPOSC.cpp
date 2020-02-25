#include <unistd.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <list>
#include <vector>
#include <sstream>
#include <httpserver.hpp>
#include <jsoncpp/json/json.h>
#include <cmath>

#include "FPPOSC.h"

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "log.h"

#include "tinyexpr.h"

enum class ParamType {
    FLOAT,
    INT,
    STRING
};

class OSCParam {
public:
    
    OSCParam() {
    }
    OSCParam(float f) {
        type = ParamType::FLOAT;
        fVal = f;
    }
    OSCParam(int f) {
        type = ParamType::INT;
        iVal = f;
    }
    OSCParam(const std::string &s) {
        type = ParamType::STRING;
        sVal = s;
    }
    
    std::string toString() {
        if (type == ParamType::STRING) {
            return sVal;
        } else if (type == ParamType::INT) {
            return std::to_string(iVal);
        }
        return std::to_string(fVal);
    }
    
    double asDouble() {
        if (type == ParamType::INT) {
            return iVal;
        }
        if (type == ParamType::FLOAT) {
            return fVal;
        }
        return atof(sVal.c_str());
    }
    
    ParamType type = ParamType::INT;
    float fVal = 0.0;
    int32_t iVal = 0;
    std::string sVal;
};

class OSCInputEvent {
public:
    OSCInputEvent(uint32_t *b) {
        int pos = 0;
        path = readString(b, pos);
        std::string type = readString(b, pos);
        LogDebug(VB_PLUGIN, "   %s - %s\n", path.c_str(), type.c_str());

        for (int y = 1; y < type.length(); y++) {
            switch (type[y]) {
                case 'i': {
                    int32_t t = be32toh(b[pos]);
                    pos++;
                    params.push_back(OSCParam(t));
                    LogDebug(VB_PLUGIN, "         %d: %d\n", y, t);
                    break;
                }
                case 'f': {
                    int t = be32toh(b[pos]);
                    pos++;
                    float *f = (float*)&t;
                    params.push_back(OSCParam(*f));
                    LogDebug(VB_PLUGIN, "         %d: %f\n", y, *f);
                    break;
                }
                case 's': {
                    std::string val = readString(b, pos);
                    params.push_back(OSCParam(val));
                    LogDebug(VB_PLUGIN, "         %d: %f\n", y, val.c_str());
                    break;
                }
            default:
                    HexDump("Received data:", (void*)&b[pos], 64);
            }
        }
    }
    std::string toString() {
        std::string v = path;
        v += "(";
        int cnt = 0;
        for (auto &a : params) {
            if (cnt != 0) {
                v += ", ";
            }
            v += a.toString();
            cnt++;
        }
        v += ")";
        return v;
    }
    static std::string readString(uint32_t* b, int &pos) {
        std::string s = (const char *)&b[pos];
        int len = s.length() / 4;
        len += 1;
        pos += len;
        return s;
    }
    std::string path;
    std::vector<OSCParam> params;
};


class OSCCondition {
public:
    OSCCondition(Json::Value &v) {
        conditionType = v["condition"].asString();
        compareType = v["conditionCompare"].asString();
        text = v["conditionText"].asString();
    }
    
    bool matches(OSCInputEvent &ev) {
        if (conditionType == "ALWAYS") {
            return true;
        }
        int idx = conditionType[1] - '1';
        if (idx >= ev.params.size()) {
            return false;
        }
        return compare(ev.params[idx]);
    }
    bool compare(OSCParam &p) {
        if (p.type == ParamType::STRING) {
            return compareS(p.sVal);
        }
        if (p.type == ParamType::FLOAT) {
            return compareF(p.fVal);
        }
        return compareI(p.iVal);
    }
    bool compareS(const std::string &s) {
        if (compareType == "=") {
            return s == text;
        } else if (compareType == "!=") {
            return s != text;
        } else if (compareType == ">=") {
            return s >= text;
        } else if (compareType == "<=") {
            return s <= text;
        } else if (compareType == ">") {
            return s > text;
        } else if (compareType == "<") {
            return s < text;
        } else if (compareType == "contains") {
            return s.find(text) != std::string::npos;
        } else if (compareType == "iscontainedin") {
            return text.find(s) != std::string::npos;
        }
        return false;
    }
    bool compareF(float f) {
        float tf = std::stof(text);
        if (compareType == "=") {
            return f == tf;
        } else if (compareType == "!=") {
            return f != tf;
        } else if (compareType == ">=") {
            return f >= tf;
        } else if (compareType == "<=") {
            return f <= tf;
        } else if (compareType == ">") {
            return f > tf;
        } else if (compareType == "<") {
            return f < tf;
        } else if (compareType == "contains") {
            return false;
        } else if (compareType == "iscontainedin") {
            return false;
        }
        return false;
    }
    bool compareI(int32_t f) {
        int tf = std::stoi(text);
        if (compareType == "=") {
            return f == tf;
        } else if (compareType == "!=") {
            return f != tf;
        } else if (compareType == ">=") {
            return f >= tf;
        } else if (compareType == "<=") {
            return f <= tf;
        } else if (compareType == ">") {
            return f > tf;
        } else if (compareType == "<") {
            return f < tf;
        } else if (compareType == "contains") {
            return false;
        } else if (compareType == "iscontainedin") {
            return false;
        }
        return false;
    }

    std::string conditionType;
    std::string compareType;
    std::string text;
};

class OSCCommandArg {
public:
    OSCCommandArg(const std::string &t) : arg(t) {
    }
    ~OSCCommandArg() {
    }
    
    std::string arg;
    std::string type;

    te_expr *expr = nullptr;
};

static const char *vNames[] = {"p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9"};

class OSCEvent {
public:
    OSCEvent(Json::Value &v) {
        path = v["path"].asString();
        description = v["description"].asString();
        for (int x = 0; x < v["conditions"].size(); x++) {
            conditions.push_back(OSCCondition(v["conditions"][x]));
        }

        command = v["command"].asString();
        for (int x = 0; x < v["args"].size(); x++) {
            args.push_back(OSCCommandArg(v["args"][x].asString()));
        }
        if (v.isMember("argTypes")) {
            for (int x = 0; x < v["argTypes"].size(); x++) {
                args[x].type = v["argTypes"][x].asString();
            }
        }
    }
    
    bool matches(OSCInputEvent &ev) {
        if (ev.path != path) {
            return false;
        }
        for (auto &c : conditions) {
            if (!c.matches(ev)) {
                return false;
            }
        }
        return true;
    }
    
    void invoke(OSCInputEvent &ev) {
        if (!exprEvaluated) {
            for (int x = 0; x < 9; x++) {
                exprVars[x].type = TE_VARIABLE;
                exprVars[x].name = vNames[x];
                exprVars[x].address = &varVals[x];
                exprVars[x].context = nullptr;
            }
            for (auto &a : args) {
                int err = 0;
                a.expr = te_compile(a.arg.c_str(), &exprVars[0], 9, &err);
                if (a.expr) {
                    hasExpr = true;
                }
            }
            exprEvaluated = true;
        }
        if (hasExpr) {
            for (int x = 0; x < ev.params.size(); x++) {
                varVals[x] = ev.params[x].asDouble();
            }
        }
        
        std::vector<std::string> ar;
        for (auto &a : args) {
            if (a.expr) {
                double d = te_eval(a.expr);
                if (a.type == "int") {
                    int i = std::round(d);
                    ar.push_back(std::to_string(i));
                } else if (a.type == "bool") {
                    ar.push_back(d != 0.0 ? "true" : "false");
                } else {
                    ar.push_back(std::to_string(d));
                }
            } else {
                ar.push_back(a.arg);
            }
        }

        CommandManager::INSTANCE.run(command, ar);
    }
    
    std::string path;
    std::string description;
    
    std::list<OSCCondition> conditions;
    
    std::string command;
    std::vector<OSCCommandArg> args;
    
        
    bool exprEvaluated = false;
    bool hasExpr = false;
    std::array<double, 9> varVals;
    std::array<te_variable, 9> exprVars;
};


class FPPOSCPlugin : public FPPPlugin, public httpserver::http_resource {
public:
    int port = 9000;
    
    #define MAX_MSG 48
    #define BUFSIZE 1500
    struct mmsghdr msgs[MAX_MSG];
    struct iovec iovecs[MAX_MSG];
    unsigned char buffers[MAX_MSG][BUFSIZE+1];
    struct sockaddr_in inAddress[MAX_MSG];

    std::list<OSCEvent> events;
    std::list<OSCInputEvent> lastEvents;
    
    FPPOSCPlugin() : FPPPlugin("fpp-osc") {
        LogInfo(VB_PLUGIN, "Initializing OSC Plugin\n");
        
        memset(msgs, 0, sizeof(msgs));
        for (int i = 0; i < MAX_MSG; i++) {
            iovecs[i].iov_base         = buffers[i];
            iovecs[i].iov_len          = BUFSIZE;
            msgs[i].msg_hdr.msg_iov    = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
            memset(buffers[i], 0, BUFSIZE);
        }
        
        if (FileExists("/home/fpp/media/config/plugin.fpp-osc.json")) {
            std::ifstream t("/home/fpp/media/config/plugin.fpp-osc.json");
            std::stringstream buffer;
            buffer << t.rdbuf();
            std::string config = buffer.str();
            Json::Value root;
            Json::Reader reader;
            bool success = reader.parse(buffer.str(), root);
            
            if (root.isMember("port")) {
                port = root["port"].asInt();
            }
            if (root.isMember("events")) {
                for (int x = 0; x < root["events"].size(); x++) {
                    events.push_back(OSCEvent(root["events"][x]));
                }
            }
        }
    }
    virtual ~FPPOSCPlugin() {
    }


    virtual const httpserver::http_response render_GET(const httpserver::http_request &req) override {
        std::string v;
        for (auto &a : lastEvents) {
            v += a.toString() + "\n";
        }
        return httpserver::http_response_builder(v, 200);
            }
    bool ProcessPacket(int i) {
        LogDebug(VB_PLUGIN, "OSC Process Packet\n");
        int msgcnt = recvmmsg(i, msgs, MAX_MSG, 0, nullptr);
        while (msgcnt > 0) {
            for (int x = 0; x < msgcnt; x++) {
                if (buffers[x][0] == '/') {
                    //osc message
                    uint32_t *b = (uint32_t *)buffers[x];
                    OSCInputEvent event(b);
                    if (lastEvents.size() > 10) {
                        lastEvents.pop_front();
                    }
                    lastEvents.push_back(event);
                    
                    for (auto &a : events) {
                        if (a.matches(event)) {
                            a.invoke(event);
                        }
                    }
                    
                } else {
                    //osc bundle?
                }
            }
            msgcnt = recvmmsg(i, msgs, MAX_MSG, 0, nullptr);
        }

        return false;
    }
    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/OSC", this, true);
    }
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {
        int sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        
        struct sockaddr_in addr;
        socklen_t addrlen;

        memset((char *)&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        addrlen = sizeof(addr);
        // Bind the socket to address/port
        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            LogDebug(VB_PLUGIN, "OSC bind failed: %s\n", strerror(errno));
            exit(1);
        }
        callbacks[sock] = [this](int i) {
            return ProcessPacket(i);
        };
    }
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPOSCPlugin();
    }
}
