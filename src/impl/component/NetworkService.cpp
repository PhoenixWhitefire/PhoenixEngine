// Network service, 22/02/2026
#include <curl/curl.h>
#include <curl/easy.h>

#include "component/NetworkService.hpp"
#include "ThreadManager.hpp"

// https://github.com/luau-lang/lute/blob/ed6e075979b85c4fd91f7648ac9a11a041a89e4c/lute/net/src/net.cpp#L51
static size_t writeFunction(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    std::vector<char>* target = static_cast<std::vector<char>*>(userdata);
    assert(target);

    size_t fullsize = size * nmemb;
    target->insert(target->end(), ptr, ptr + fullsize);
    return fullsize;
}

static void makeHttpRequest(std::vector<Reflection::GenericValue> Inputs, std::promise<std::vector<Reflection::GenericValue>>* Out)
{
    std::unordered_map<Reflection::GenericValue, Reflection::GenericValue> returnTable;

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        returnTable["Ok"] = false;
        returnTable["Error"] = "Failed to initialize Curl";

        Out->set_value({ returnTable });
        return;
    }

    const std::string_view& url = Inputs[0].AsStringView();
    std::vector<char> responseBody;

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1l);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    if (Inputs.size() > 1)
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, Inputs[1].AsStringView().data());

    if (Inputs.size() > 2)
    {
        const std::string_view& body = Inputs[2].AsStringView();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    }

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK)
    {
        returnTable["Ok"] = false;
        returnTable["Error"] = curl_easy_strerror(result);
        Out->set_value({ returnTable });

        curl_easy_cleanup(curl);
        return;
    }

    int64_t responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    returnTable["Ok"] = responseCode >= 200 && responseCode < 300;
    returnTable["Status"] = responseCode;
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
                { Reflection::ValueType::String, REFLECTION_OPTIONAL(String), REFLECTION_OPTIONAL(String) },
                { Reflection::ValueType::Map },
                [](void*, const std::vector<Reflection::GenericValue>& inputs) -> std::promise<std::vector<Reflection::GenericValue>>*
                {
                    std::promise<std::vector<Reflection::GenericValue>>* prom = new std::promise<std::vector<Reflection::GenericValue>>;

                    ThreadManager::Get()->Dispatch(
                        [=]()
                        {
                            makeHttpRequest(inputs, prom);
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
