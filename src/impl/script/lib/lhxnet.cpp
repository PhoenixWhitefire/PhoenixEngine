#include <optional>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <curl/curl.h>

#include "script/luhx.hpp"
#include "script/ScriptEngine.hpp"
#include "ThreadManager.hpp"

static_assert(sizeof(long) == sizeof(int64_t));

// 18/08/2025
// https://github.com/luau-lang/lute/blob/primary/lute/net/src/net.cpp
struct CurlResponse
{
    std::string error;
    std::vector<char> body;
    std::unordered_map<std::string, std::string> headers;
    // INT64_MAX is used to indicate the request is still in progress
    int64_t status = INT64_MAX;
};

static size_t writeFunction(void* contents, size_t size, size_t nmemb, void* context)
{
    std::vector<char>& target = *(std::vector<char>*)context;
    size_t fullsize = size * nmemb;

    target.insert(target.end(), (char*)contents, (char*)contents + fullsize);

    return fullsize;
}

static CurlResponse requestData(
    const std::string& url,
    const std::string& method,
    const std::string& body,
    const std::vector<std::pair<std::string, std::string>>& headers
)
{
    CURL* curl = curl_easy_init();
    CurlResponse resp;
    if (!curl)
    {
        resp.error = "failed to initialize";
        resp.status = 44; // Obsolete error
        return resp;
    }

    std::vector<char> data;
    std::vector<char> headerData;
    curl_slist* headerList = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    if (method != "GET")
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

    if (!body.empty())
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    if (!headers.empty())
    {
        for (const auto& header_pair : headers)
        {
            std::string header_str = header_pair.first + ": " + header_pair.second;
            headerList = curl_slist_append(headerList, header_str.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);
    resp.status = res;

    if (headerList)
        curl_slist_free_all(headerList);

    if (res != CURLE_OK)
    {
        resp.error = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        return resp;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);

    resp.body = std::move(data);

    curl_header* prev = nullptr;
    curl_header* h;

    while ((h = curl_easy_nextheader(curl, CURLH_HEADER, 0, prev)))
    {
        std::string name = h->name;
        std::string value = h->value;

        if (resp.headers.contains(name))
        {
            resp.headers[name] += ", " + value;
        }
        else
        {
            resp.headers[name] = value;
        }
        prev = h;
    }

    curl_easy_cleanup(curl);
    return resp;
}

static int net_request(lua_State* L)
{
    // 18/08/2025 reference implementation:
    // https://github.com/luau-lang/lute/blob/a58e4e11a4ef4f2f5c2d2611ddfc8053a6ba3577/lute/net/src/net.cpp#L120

    std::string url = luaL_checkstring(L, 1);
    std::string method = "GET";
    std::string body = "";
    std::vector<std::pair<std::string, std::string>> headers;

    if (lua_istable(L, 2))
    {
        lua_getfield(L, 2, "method");
        if (lua_isstring(L, -1))
            method = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 2, "body");
        if (lua_isstring(L, -1))
            body = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 2, "headers");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            while (lua_next(L, -2))
            {
                if (lua_isstring(L, -2) && lua_isstring(L, -1))
                {
                    std::string key = lua_tostring(L, -2);
                    std::string value = lua_tostring(L, -1);
                    headers.emplace_back(key, value);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    ScriptEngine::L::Yield(
        L,
        1,
        [&](ScriptEngine::YieldedCoroutine& yc)
        {
            std::mutex* responseMutex = new std::mutex;
            CurlResponse* response = new CurlResponse;

            ThreadManager::Get()->Dispatch(
                [=]()
                {
                    CurlResponse resp = requestData(url, method, body, headers);

                    responseMutex->lock();
                    *response = resp;
                    responseMutex->unlock();
                },
                true
            );

            yc.Mode = ScriptEngine::YieldedCoroutine::ResumptionMode::Polled;
            yc.RmPoll = [response, responseMutex](lua_State* CL)
                {
                    std::unique_lock<std::mutex> lock(*responseMutex);
                
                    // not done yet
                    if (response->status == INT64_MAX)
                        return -1;

                    lua_createtable(CL, 0, 4);
                
                    lua_pushlstring(CL, response->body.data(), response->body.size());
                    lua_setfield(CL, -2, "body");
                
                    lua_createtable(CL, 0, response->headers.size());
                    for (const auto& header : response->headers)
                    {
                        lua_pushlstring(CL, header.first.data(), header.first.size());
                        lua_pushlstring(CL, header.second.data(), header.second.size());
                        lua_settable(CL, -3);
                    }
                    lua_setfield(CL, -2, "headers");
                
                    lua_pushinteger(CL, response->status);
                    lua_setfield(CL, -2, "status");
                
                    lua_pushboolean(CL, (response->status >= 200 && response->status < 300));
                    lua_setfield(CL, -2, "ok");
                
                    lua_pushlstring(CL, response->error.data(), response->error.size());
                    lua_setfield(CL, -2, "error");
                
                    return 1;
                };
        }
    );

    return -1;
}

struct CurlHolder
{
    CurlHolder()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlHolder()
    {
        curl_global_cleanup();
    }
};

static CurlHolder& globalCurlInit()
{
    static CurlHolder holder;
    return holder;
}

static luaL_Reg net_funcs[] =
{
    { "request", net_request },
    /* { "serve", netserve }, */
    { NULL, NULL }
};

int luhxopen_net(lua_State* L)
{
    globalCurlInit();

    luaL_register(L, LUHX_NETLIBNAME, net_funcs);

    return 1;
}
