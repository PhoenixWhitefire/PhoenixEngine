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
    HttpRequest request;
    bool gotUrl = false;

    gv.ForEachMapPair([&](const Reflection::GenericValue& key, const Reflection::GenericValue& value)
    {
        const std::string_view& field = key.AsStringView();

        if (field == "Url")
        {
            request.Url = value.AsString();
            gotUrl = true;
        }
        else if (field == "Method")
            request.Method = value.AsString();
        else if (field == "Headers")
        {
            value.ForEachMapPair([&](const Reflection::GenericValue& headerKey, const Reflection::GenericValue& headerValue)
            {
                request.Headers.emplace_back(headerKey.AsString(), headerValue.AsString());
            });
        }
        else if (field == "Body")
            request.Body = value.AsString();
        else
            RAISE_RT("Invalid field '{}' with value '{}' ()", field, value.ToString(), Reflection::TypeAsString(value.Type));
    });

    if (!gotUrl)
        RAISE_RT("Expected `Url` field in request data");

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
    std::vector<std::pair<Reflection::GenericValue, Reflection::GenericValue>> returnTable;

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        Out->set_value({ Reflection::GenericValue::MapPairs({
            { "Ok", false },
            { "Status", 0 },
            { "Body", "Failed to initialize Curl" },
        }) });

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
        Out->set_value({ Reflection::GenericValue::MapPairs({
            { "Ok", false },
            { "Status", 0 },
            { "Body", curl_easy_strerror(result) },
        }) });

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

    Out->set_value({ Reflection::GenericValue::MapPairs({
        { "Ok", responseCode >= 200 && responseCode < 300 },
        { "Status", responseCode },
        { "Headers", headers },
        { "Body", std::string_view(responseBody.begin(), responseBody.end()) },
    }) });

    curl_easy_cleanup(curl);
}

const Reflection::StaticMethodMap& NetworkComponentManager::GetMethods()
{
    static const Reflection::StaticMethodMap methods = {
        { "MakeHttpRequestAsync", Reflection::MethodDescriptor{
            REFLECTION_SPAN({ Reflection::ValueType::Map }),
            REFLECTION_SPAN({ Reflection::ValueType::Map }),
            [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::promise<std::vector<Reflection::GenericValue>>*
            {
                std::promise<std::vector<Reflection::GenericValue>>* prom = new std::promise<std::vector<Reflection::GenericValue>>;
                HttpRequest request = parseRequestFromReflection(inputs[0]);

                ThreadManager::Get()->Dispatch(
                    "HttpRequest",
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
