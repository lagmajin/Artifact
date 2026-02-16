module;
export module AI.Client;

import std;
import Utils.String.UniString;

export namespace Artifact {

class AIClient {
private:
    class Impl;
    Impl* impl_;
public:
    AIClient();
    ~AIClient();

    // singleton
    static AIClient* instance();

    // configuration
    void setApiKey(const UniString& key);
    void setProvider(const UniString& provider);

    // Send a message to AI and get a (dummy) response synchronously for now
    UniString sendMessage(const UniString& message);
};

}
