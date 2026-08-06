#include <fpp-pch.h>

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
#include <SysSocket.h>
#include <cmath>


#include "FPPOSC.h"

#include "commands/Commands.h"
#include "common.h"
#include "settings.h"
#include "Plugin.h"
#include "fpphttp.h"
#include "log.h"
#include "Warnings.h"
#include "EPollManager.h"
#include "FileMonitor.h"

#include "util/ExpressionProcessor.h"

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
                    LogDebug(VB_PLUGIN, "     Unknown Type %d: %d\n", y, (int)type[y]);
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
        if (processor) {
            delete processor;
        }
    }
    
    std::string arg;
    std::string type;
    
    ExpressionProcessor *processor = nullptr;
    
    std::string evaluate(const std::string &tp) {
        if (processor) {
            std::string s = processor->evaluate(tp);
            return s;
        }
        return "";
    }

};

static std::string vNames[] = {"p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9"};

class OSCEvent {
public:
    OSCEvent(Json::Value &v) {
        path = v["path"].asString();
        description = v["description"].asString();
        for (int x = 0; x < v["conditions"].size(); x++) {
            conditions.push_back(OSCCondition(v["conditions"][x]));
        }

        command = v;
        command.removeMember("path");
        command.removeMember("description");
        command.removeMember("conditions");
        command.removeMember("argTypes");
        command.removeMember("args");

        for (int x = 0; x < v["args"].size(); x++) {
            args.push_back(OSCCommandArg(v["args"][x].asString()));
        }
        if (v.isMember("argTypes")) {
            for (int x = 0; x < v["argTypes"].size(); x++) {
                args[x].type = v["argTypes"][x].asString();
            }
        }
        for (auto &a : args) {
            a.processor = new ExpressionProcessor();
        }
        for (int x = 0; x < 9; x++) {
            ExpressionProcessor::ExpressionVariable *var = new ExpressionProcessor::ExpressionVariable(vNames[x]);
            variables[x] = var;
            for (auto &a : args) {
                a.processor->bindVariable(var);
            }
        }
        for (auto &a : args) {
            a.processor->compile(a.arg);
        }
    }
    ~OSCEvent() {
        conditions.clear();
        args.clear();
        for (int x = 0; x < 9; x++) {
            delete variables[x];
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
        for (int x = 0; x < ev.params.size(); x++) {
            variables[x]->setValue(ev.params[x].toString());
        }
        
        Json::Value newCommand = command;;

        for (auto &a : args) {
            std::string tp = "string";
            if (a.type == "bool" || a.type == "int") {
                tp = a.type;
            }
            
            //printf("Eval p: %s\n", a.arg.c_str());
            std::string r = a.evaluate(tp);
            //printf("        -> %s\n", r.c_str());
            newCommand["args"].append(r);
        }

        CommandManager::INSTANCE.run(newCommand);
    }
    
    std::string path;
    std::string description;
    
    std::list<OSCCondition> conditions;
    
    Json::Value command;
    std::vector<OSCCommandArg> args;
            
    std::array<ExpressionProcessor::ExpressionVariable*, 9> variables;
};

class OSCCommand : public Command {
public:
    OSCCommand(int sock) : Command("OSC Event"), socket(sock) {
        args.push_back(CommandArg("Path", "string", "Path"));
        args.push_back(CommandArg("IPAddress", "string", "IP Address"));
        args.push_back(CommandArg("Port", "int", "Port").setRange(1, 65535).setDefaultValue("9000"));
        for (int x = 1; x < 5; x++) {
            std::string n = "P";
            n += std::to_string(x);
            args.push_back(CommandArg(n + "Type", "string", n + " Type").setContentList({"None", "Integer", "Float", "String"}).setDefaultValue("None"));
            args.push_back(CommandArg(n, "string", n));
        }
    }
    
    int roundTo4(int p) {
        while (p % 4) {
            p++;
        }
        return p;
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        char buf[1200];
        memset(buf, 0, 1200);
        strcpy(buf, args[0].c_str());
        int pos = args[0].length();
        pos = roundTo4(pos);
        
        std::string types = ",";
        
        for (int x = 0; x < 4; x++) {
            std::string type = args[x*2 + 3];
            if (type == "Integer") {
                types += "i";
            } else if (type == "Float") {
                types += "f";
            } else if (type == "String") {
                types += "s";
            }
        }
        strcpy(&buf[pos], types.c_str());
        pos += types.length();
        pos = roundTo4(pos);
        for (int x = 0; x < 4; x++) {
            std::string type = args[x*2 + 3];
            std::string value = args[x*2 + 4];
            if (type == "Integer") {
                int i = 0;
                try {
                    i = std::stoi(value);
                } catch (...) {
                    
                }
                i = htobe32(i);
                memcpy(&buf[pos], &i, 4);
                pos += 4;
            } else if (type == "Float") {
                int i = 0;
                try {
                    float f = std::stof(value);
                    int *ip = (int*)&f;
                    i = *ip;
                } catch (...) {
                    
                }
                i = htobe32(i);
                memcpy(&buf[pos], &i, 4);
                pos += 4;
            } else if (type == "String") {
                strcpy(&buf[pos], value.c_str());
                pos += value.length();
                pos = roundTo4(pos);
            }
        }
        int port = 9000;
        if (args[2] != "") {
            port = std::stoi(args[2]);
        }
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = inet_addr(args[1].c_str());
        dest_addr.sin_port = htons(port);
        
        sendto(socket, buf, pos, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        return std::make_unique<Command::Result>("OSC Command Sent");
    }
    
    int socket;
};


class FPPOSCPlugin : public FPPPlugins::Plugin, public FPPPlugins::APIProviderPlugin {
public:
    int port = 9000;
    
    #define MAX_MSG 48
    #define BUFSIZE 1500
    struct mmsghdr msgs[MAX_MSG];
    struct iovec iovecs[MAX_MSG];
    unsigned char buffers[MAX_MSG][BUFSIZE+1];
    struct sockaddr_in inAddress[MAX_MSG];

    std::list<OSCEvent *> events;
    std::list<OSCInputEvent> lastEvents;
    
    FPPOSCPlugin() : FPPPlugins::Plugin("fpp-osc"), FPPPlugins::APIProviderPlugin() {
        LogInfo(VB_PLUGIN, "Initializing OSC Plugin\n");
        
        memset(msgs, 0, sizeof(msgs));
        for (int i = 0; i < MAX_MSG; i++) {
            iovecs[i].iov_base         = buffers[i];
            iovecs[i].iov_len          = BUFSIZE;
            msgs[i].msg_hdr.msg_iov    = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
            memset(buffers[i], 0, BUFSIZE);
        }
        
        loadConfig();

        // This plugin's configuration is its own JSON file rather than the
        // key=value settings file FPPPlugins::Plugin watches, so the
        // monitorSettings constructor argument would not see it. Watch it
        // directly instead, so editing the event map or the port takes effect
        // without restarting fppd. shutdown() gives the watch back - the
        // callback lives in this library.
        std::function<void()> reload = [this]() {
            configChanged();
        };
        FileMonitor::INSTANCE.AddFile(name, FPP_DIR_CONFIG("/plugin.fpp-osc.json"), reload);
    }

    // Replace the event map, and the port if it moved. Runs on the main loop.
    void configChanged() {
        int oldPort = port;
        LogInfo(VB_PLUGIN, "OSC: configuration changed, reloading\n");
        loadConfig();
        if (port != oldPort) {
            LogInfo(VB_PLUGIN, "OSC: port %d -> %d, rebinding\n", oldPort, port);
            closeListenSocket();
            openListenSocket();
        }
    }

    void loadConfig() {
        for (auto e : events) {
            delete e;
        }
        events.clear();
        if (FileExists(FPP_DIR_CONFIG("/plugin.fpp-osc.json"))) {
            Json::Value root;
            LoadJsonFromFile(FPP_DIR_CONFIG("/plugin.fpp-osc.json"), root);
            if (root.isMember("port")) {
                port = root["port"].asInt();
            }
            if (root.isMember("events")) {
                for (int x = 0; x < root["events"].size(); x++) {
                    events.push_back(new OSCEvent(root["events"][x]));
                }
            }
        }
    }
    virtual ~FPPOSCPlugin() {
        for (auto e : events) {
            delete e;
        }
    }

    // Give back everything addControlCallbacks() took. Closing the listen socket
    // is not optional: FPP takes the descriptor out of its epoll loop on unload
    // but the socket stays open and the UDP port stays bound, so the next load
    // would bind() the same port, get EADDRINUSE and take fppd down with it -
    // the bind failure path below is exit(1). Also withdraws the command, whose
    // vtable lives in this .so. Nothing here is asynchronous, so no readiness
    // predicate is needed.
    virtual std::function<bool()> shutdown() override {
        if (oscCommand) {
            // removeCommand() only unregisters - CommandManager deletes whatever
            // is still in its registry at shutdown, so taking it back means
            // owning it again.
            CommandManager::INSTANCE.removeCommand(oscCommand);
            delete oscCommand;
            oscCommand = nullptr;
        }
        FileMonitor::INSTANCE.RemoveFile(name, FPP_DIR_CONFIG("/plugin.fpp-osc.json"));
        closeListenSocket();
        return nullptr;
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
                        if (a->matches(event)) {
                            a->invoke(event);
                        }
                    }
                } else if (strncmp("#bundle", (const char *)&buffers[x][0], 7) == 0) {
                    LogDebug(VB_PLUGIN, "Found #bundle Packet: msglen: %d\n", msgs[x].msg_len);
                    uint8_t *data = buffers[x];
                    HexDump("Msg Data: ", data, msgs[x].msg_len, VB_PLUGIN, 32);
                    //osc bundle
                    // first 8 are "#bundle" + null
                    // next 8 are timecode
                    int idx = 16;
                    uint32_t *b = (uint32_t *)(&data[idx]);
                    uint32_t sz = b[0];
                    idx += 4;
                    b = (uint32_t *)(&data[idx]);
                    LogDebug(VB_PLUGIN, "    Index %d:  Size: %d    First: %X\n", idx, sz, b[0]);
                    while (sz > 0 && idx < msgs[x].msg_len) {
                        OSCInputEvent event(b);
                        if (lastEvents.size() > 10) {
                            lastEvents.pop_front();
                        }
                        lastEvents.push_back(event);
                        
                        for (auto &a : events) {
                            if (a->matches(event)) {
                                a->invoke(event);
                            }
                        }
                        idx += sz;
                        if (idx < msgs[x].msg_len) {
                            b = (uint32_t *)(&data[idx]);
                            sz = b[0];
                            idx += 4;
                            b = (uint32_t *)(&data[idx]);
                            LogDebug(VB_PLUGIN, "    Index %d:  Size: %d    First: %X\n", idx, sz, b[0]);
                        }
                    }
                } else {
                    LogDebug(VB_PLUGIN, "Unknown OSC packet  %02X %02X %02X %02X\n", buffers[x][0], buffers[x][1], buffers[x][2], buffers[x][3]);
                }
            }
            msgcnt = recvmmsg(i, msgs, MAX_MSG, 0, nullptr);
        }

        return false;
    }
    void unregisterApis() override {
        // Disarms /OSC and its subpaths and does not return until no request is
        // inside the handler and the handler itself - this plugin's code - has
        // been destroyed, which is what makes a later dlclose() safe.
        FPPPlugins::unregisterPluginApi("/OSC");
    }
    void registerApis() override {
        auto handleOSC = [this](const HttpRequestPtr& req,
                                HttpCallback &&callback) {
            std::string v;
            for (auto &a : lastEvents) {
                v += a.toString() + "\n";
            }
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k200OK);
            resp->setContentTypeString("text/plain");
            resp->setBody(v);
            callback(resp);
        };
        // Only "/OSC" paths are needed: Apache rewrites api/plugin-apis/OSC to
        // localhost:32322/OSC, stripping the plugin-apis/ prefix, so a
        // "/api/plugin-apis/OSC" route would never be reached.
        // The UI fetches api/plugin-apis/OSC/Last, and the pre-drogon web
        // server matched the whole /OSC subtree, so family=true registers the
        // sub-paths too.
        //
        // Registered through FPP rather than drogon::app() directly: drogon has
        // no route removal, so a handler registered straight with it could never
        // be withdrawn and would pin this plugin in memory for the life of fppd.
        FPPPlugins::registerPluginApi("/OSC", std::move(handleOSC), {drogon::Get}, true);
    }
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) override {
        // Nothing goes into the callbacks map: the listen socket is rebound when
        // the configured port changes, and the map is long gone by then. It is
        // this plugin's own descriptor, so it registers and withdraws it itself.
        openListenSocket();
    }

    void openListenSocket() {
        int sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

        struct sockaddr_in addr;
        memset((char *)&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        // Bind the socket to address/port. A failure here used to exit(1), which
        // was survivable when this only ever ran during fppd startup. It is not
        // now: this also runs when the plugin is installed, reloaded, or given a
        // new port on a running player, so a busy port would take the show down.
        // Log it and leave the plugin loaded but not listening instead.
        if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            LogErr(VB_PLUGIN, "OSC bind to port %d failed: %s - OSC input disabled\n", port, strerror(errno));
            close(sock);
            WarningHolder::AddWarning("fpp-osc could not listen on UDP port " + std::to_string(port) + ": " + strerror(errno));
            return;
        }
        WarningHolder::RemoveWarning("fpp-osc could not listen on UDP port " + std::to_string(port) + ": Address already in use");
        listenSocket = sock;
        std::function<bool(int)> cb = [this](int i) {
            return ProcessPacket(i);
        };
        EPollManager::INSTANCE.addFileDescriptor(listenSocket, cb);

        if (oscCommand == nullptr) {
            oscCommand = new OSCCommand(listenSocket);
            CommandManager::INSTANCE.addCommand(oscCommand);
        } else {
            oscCommand->socket = listenSocket;
        }
    }

    // Closing the socket is not optional when the port moves: it stays bound
    // otherwise, and the rebind gets EADDRINUSE.
    void closeListenSocket() {
        if (listenSocket >= 0) {
            EPollManager::INSTANCE.removeFileDescriptor(listenSocket);
            close(listenSocket);
            listenSocket = -1;
        }
    }

    int listenSocket = -1;
    OSCCommand *oscCommand = nullptr;
};


// Safe to dlclose() on unload: no threads, no timers, no CurlManager requests
// and no drogon client objects. The routes go through registerPluginApi() and
// come back in unregisterApis(); FPP withdraws the listen descriptor from its
// epoll loop, and shutdown() closes that socket and withdraws the command. The
// OSCEvent objects are this plugin's own and go in the destructor, which runs
// before the library is unmapped.
FPP_PLUGIN_SUPPORTS_UNLOAD()

extern "C" {
    FPPPlugins::Plugin *createPlugin() {
        return new FPPOSCPlugin();
    }
}
