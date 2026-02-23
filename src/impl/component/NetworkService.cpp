// Network service, 22/02/2026
#include <curl/curl.h>
#include <curl/easy.h>

#include "component/NetworkService.hpp"
#include "ThreadManager.hpp"
#include "Utilities.hpp"

struct HttpRequest
{
    std::string Url;
    std::string Method;
    std::vector<std::pair<std::string, std::string>> Headers;
    std::string Body;
};

static HttpRequest parseRequestFromReflection(const Reflection::GenericValue& gv)
{
    std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> map = gv.AsMap();
    const auto& urlIt = map.find("Url");
    const auto& methodIt = map.find("Method");
    const auto& headersIt = map.find("Headers");
    const auto& bodyIt = map.find("Body");

    if (urlIt == map.end())
        RAISE_RT("Expected `Url` field in request data");
    if (urlIt->second.Type != Reflection::ValueType::String)
        RAISE_RTF("Expected `Url` to be a string, got '{}' ({})", urlIt->second.ToString(), Reflection::TypeAsString(urlIt->second.Type));

    HttpRequest request;
    request.Url = urlIt->second.AsString();

    if (methodIt != map.end())
    {
        if (methodIt->second.Type != Reflection::ValueType::String)
            RAISE_RTF("Expected `Method` to be a string, got '{}' ({})", methodIt->second.ToString(), Reflection::TypeAsString(methodIt->second.Type));
        request.Method = methodIt->second.AsString();
    }

    if (headersIt != map.end())
    {
        if (headersIt->second.Type != Reflection::ValueType::Map && headersIt->second.Type != Reflection::ValueType::Array)
            RAISE_RTF("Expected `Body` to be a string, got '{}' ({})", bodyIt->second.ToString(), Reflection::TypeAsString(bodyIt->second.Type));

        std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> headersGv = headersIt->second.AsMap();
        for (const auto& pair : headersGv)
        {
            if (pair.first.Type != Reflection::ValueType::String)
                RAISE_RTF("Expected Key of Header to be a string, got '{}' ({})", pair.first.ToString(), Reflection::TypeAsString(pair.first.Type));
            if (pair.second.Type != Reflection::ValueType::String)
                RAISE_RTF("Expected Value of Header to be a string, got '{}' ({})", pair.second.ToString(), Reflection::TypeAsString(pair.second.Type));

            request.Headers.emplace_back(pair.first.AsString(), pair.second.AsString());
        }
    }

    if (bodyIt != map.end())
    {
        if (bodyIt->second.Type != Reflection::ValueType::String)
            RAISE_RTF("Expected `Body` to be a string, got '{}' ({})", bodyIt->second.ToString(), Reflection::TypeAsString(bodyIt->second.Type));
        request.Body = bodyIt->second.AsString();
    }

    return request;
}

// https://github.com/luau-lang/lute/blob/ed6e075979b85c4fd91f7648ac9a11a041a89e4c/lute/net/src/net.cpp#L51
static size_t writeFunction(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::vector<char>* target = static_cast<std::vector<char>*>(userdata);
    assert(target);

    size_t fullsize = size * nmemb;
    target->insert(target->end(), ptr, ptr + fullsize);
    return fullsize;
}

static void makeHttpRequest(const HttpRequest& request, std::promise<std::vector<Reflection::GenericValue>>* Out)
{
    std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> returnTable;
    returnTable["Status"] = 0;

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        returnTable["Ok"] = false;
        returnTable["Error"] = "Failed to initialize Curl";

        Out->set_value({ returnTable });
        return;
    }

    std::vector<char> responseBody;

    curl_easy_setopt(curl, CURLOPT_URL, request.Url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1l);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    if (request.Method.size() > 0)
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.Method.c_str());

    if (request.Headers.size() > 0)
    {
        curl_slist* headerList = nullptr;
        for (const auto& pair : request.Headers)
        {
            std::string header_str = pair.first + ": " + pair.second;
            headerList = curl_slist_append(headerList, header_str.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    if (request.Body.size() > 0)
    {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.Body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.Body.size());
    }

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK)
    {
        returnTable["Ok"] = false;
        returnTable["Status"] = 0;
        returnTable["Error"] = curl_easy_strerror(result);
        Out->set_value({ returnTable });

        curl_easy_cleanup(curl);
        return;
    }

    int64_t responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> headers;

    curl_header* prev = nullptr;
    curl_header* h = nullptr;

    while ((h = curl_easy_nextheader(curl, CURLH_HEADER, 0, prev)))
    {
        std::string name = h->name;
        std::string value = h->value;

        if (const auto& it = headers.find(name); it != headers.end())
            it->second = it->second.AsString() + ", " + value;
        else
            headers[name] = value;

        prev = h;
    }

    returnTable["Ok"] = responseCode >= 200 && responseCode < 300;
    returnTable["Status"] = responseCode;
    returnTable["Headers"] = headers;
    returnTable["Body"] = std::string_view(responseBody.begin(), responseBody.end());
    Out->set_value({ returnTable });

    curl_easy_cleanup(curl);
}

class NetworkServiceManager : public ComponentManager<EcNetworkService>
{
public:
    const Reflection::StaticMethodMap& GetMethods() override
    {
        static const Reflection::StaticMethodMap methods = {
            { "MakeHttpRequestAsync", Reflection::MethodDescriptor{
                { Reflection::ValueType::Map },
                { Reflection::ValueType::Map },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::promise<std::vector<Reflection::GenericValue>>*
                {
                    std::promise<std::vector<Reflection::GenericValue>>* prom = new std::promise<std::vector<Reflection::GenericValue>>;
                    HttpRequest request = parseRequestFromReflection(inputs[0]);

                    ThreadManager::Get()->Dispatch(
                        [request, prom]()
                        {
                            makeHttpRequest(request, prom);
                        },
                        true
                    );

                    return prom;
                }
            } }
        };

        return methods;
    }
};

static NetworkServiceManager Instance;
