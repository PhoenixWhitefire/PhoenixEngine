#include <tracy/Tracy.hpp>
#include <iostream>
#include <fstream>
#include <format>
#include <thread>
#include <chrono>
#include <mutex>

#include "Log.hpp"
#include "component/LoggingService.hpp"
#include "Reflection.hpp"
#include "Utilities.hpp"
#include "FileRW.hpp"

static std::thread::id MainThreadId;
static std::ofstream LogHandle;
static std::string ProgramLog;

struct ParallelLogEvent
{
	std::string Message;
	std::string ExtraTags;
	Reflection::GenericValue Value;
	double Time = 0.f;
	Logging::MessageType Type;
};
static std::mutex ParallelLogEventsMutex;
static std::vector<ParallelLogEvent> ParallelLogEvents;

void Logging::Save()
{
	ZoneScoped;

	LogHandle << ProgramLog;
	LogHandle.close();
	LogHandle.open("./log.txt", std::ios_base::app);

	ProgramLog.clear();
}

static void appendToLog(const std::string_view& Message, bool NoNewline = false)
{
	ZoneScoped;

	static bool ThrewLogCapacityExceededException = false;

	if (ThrewLogCapacityExceededException)
		return;

	if ( (( Message.size() >= 2 && Message.substr(Message.size() - 2, 2) != "&&" )
		|| Message.size() < 2) && !NoNewline
	)
	{
		ProgramLog.append(Message);
		ProgramLog.append("\n");
		std::cout << Message << "\n";
	}
	else
	{
		std::string_view loggedString = !NoNewline ? Message.substr(0ull, Message.size() - 2) : Message;
		ProgramLog.append(loggedString);
		std::cout << loggedString;
	}

	// log > 24 megabytes... just in case something goes wrong
	// 10/11/2024
	if (ProgramLog.size() > 24e6)
	{
		ThrewLogCapacityExceededException = true;

		// Log Size Limit Exceeded Throwing Exception
		ProgramLog.append("\nLSLETE: Log size limit exceeded, throwing exception\n");
		Logging::Save();

		RAISE_RT("Program log exceeds maximum size of 24e6 bytes (24 megabytes)");
	}
}

static constexpr std::string_view TypeTags[] = {
	"[NONE]",
	"[INFO]",
	"[WARN]",
	"[ERRR]",
};
static_assert(std::size(TypeTags) == Logging::MessageType::Error + 1);

static void log(
	Logging::MessageType Type,
	const std::string_view& Message,
	const std::string_view& ExtraTags,
	const std::string& ContextExtraTags,
	const Reflection::GenericValue& Value = Reflection::GenericValue::Null(),
	bool TaglessAppend = false
)
{
	ZoneScoped;
	double time = GetRunningTime();
	auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());

	std::string tags = std::string(ExtraTags);
	if (ContextExtraTags.size() > 0)
		tags += (tags.size() > 0 ? "," : "") + ContextExtraTags;

	if (std::this_thread::get_id() != MainThreadId)
	{
		std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelLogEventsMutex);
		ParallelLogEvents.push_back(ParallelLogEvent{
			.Message = std::string(Message),
			.ExtraTags = tags,
			.Value = Value,
			.Time = time,
			.Type = Type,
		});
		return;
	}

	if (!TaglessAppend)
	{
		appendToLog(std::format("[{:%H:%M:%S}]", now), true);
		appendToLog(TypeTags[Type], true);
		if (!tags.empty())
		{
			appendToLog("[", true);
			appendToLog(tags, true);
			appendToLog("]", true);
		}
		appendToLog(": ", true);
	}
	appendToLog(Message, false);

	if (Logging::IsGameObjectManagerAlive)
		((LoggingComponentManager*)LoggingComponentManager::Get())->SignalNewLogMessage(time, Type, Message, tags, Value);
}

// Append message to log, which is saved to file every second
// and when app shuts down
// If the Message ends with `&&`, won't insert a newline automatically
// 11/11/2024
void Logging::Context::Append(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;
	log(Logging::MessageType::None, Message, ExtraTags, ContextExtraTags, Reflection::GenericValue::Null(), true);
}

void Logging::Context::AppendWithValue(const std::string_view& Message, const Reflection::GenericValue& Value, const std::string_view& ExtraTags) const
{
	ZoneScoped;
	log(Logging::MessageType::None, Message, ExtraTags, ContextExtraTags, Value, true);
}

void Logging::Context::Info(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;
	log(Logging::MessageType::Info, Message, ExtraTags, ContextExtraTags);
}

void Logging::Context::Warning(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;
	log(Logging::MessageType::Warning, Message, ExtraTags, ContextExtraTags);
}

void Logging::Context::Error(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;
	log(Logging::MessageType::Error, Message, ExtraTags, ContextExtraTags);
}

void Logging::Context::Write(const std::string_view& Message, MessageType Type, const std::string_view& ExtraTags) const
{
	switch (Type)
	{
	case Logging::MessageType::None:
		Log.Append(Message, ExtraTags);
		break;

	case Logging::MessageType::Info:
		Log.Info(Message, ExtraTags);
		break;

	case Logging::MessageType::Warning:
		Log.Warning(Message, ExtraTags);
		break;

	case Logging::MessageType::Error:
		Log.Error(Message, ExtraTags);
		break;

	[[unlikely]] default:
		assert(false);
		Log.Error(Message, ExtraTags);
	}
}

void Logging::Initialize()
{
	ZoneScoped;

	MainThreadId = std::this_thread::get_id();

	PHX_CHECK(FileRW::WriteFile("./log.txt", ""));
	LogHandle.open("./log.txt", std::ios_base::app);

	Log = Context{ .ContextExtraTags = "LogContext:Main" };
}

void Logging::FlushParallelEvents()
{
	ZoneScoped;

	std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelLogEventsMutex);

	for (const ParallelLogEvent& ple : ParallelLogEvents)
	{
		std::string extraTags = ple.ExtraTags;
		extraTags += ",ParallelLog";

		if (ple.Type == MessageType::None && ple.Value.Type != Reflection::ValueType::Null)
			Log.AppendWithValue(ple.Message, ple.Value, ple.ExtraTags);
		else
			Log.Write(ple.Message, ple.Type, ple.ExtraTags);
	}

	ParallelLogEvents.clear();
}

Logging::ScopedContext::ScopedContext(const Logging::Context& Ctx)
{
	m_PrevContext = Log;
	Log = Ctx;
}

Logging::ScopedContext::~ScopedContext()
{
	Log = m_PrevContext;
}
