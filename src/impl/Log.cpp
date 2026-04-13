#include <tracy/Tracy.hpp>
#include <iostream>
#include <fstream>
#include <format>
#include <thread>
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
	Logging::MessageType Type;
};
static std::mutex ParallelLogEventsMutex;
static std::vector<ParallelLogEvent> ParallelLogEvents;
static bool s_IsGameObjectManagerReady = false;

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

	// log > 2 megabytes... just in case something goes wrong
	// 10/11/2024
	if (ProgramLog.size() > 2e6)
	{
		ThrewLogCapacityExceededException = true;

		// Log Size Limit Exceeded Throwing Exception
		ProgramLog.append("\nLSLETE: Log size limit exceeded, throwing exception\n");
		Logging::Save();

		RAISE_RT("Program log exceeds maximum size of 2e6 bytes (2 megabytes)");
	}
}

// Append message to log, which is saved to file every second
// and when app shuts down
// If the Message ends with `&&`, won't insert a newline automatically
// 11/11/2024
void Logging::Context::Append(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;

	std::string tags = std::string(ExtraTags);
	if (ContextExtraTags.size() > 0)
		tags += (tags.size() > 0 ? "," : "") + ContextExtraTags;

	if (std::this_thread::get_id() != MainThreadId)
	{
		std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelLogEventsMutex);
		ParallelLogEvents.push_back(ParallelLogEvent{
			.Message = std::string(Message),
			.ExtraTags = tags,
			.Type = Logging::MessageType::None
		});
		return;
	}

	//if (ExtraTags != "")
	//	appendToLog(ExtraTags, true);
	appendToLog(Message, false);

	if (s_IsGameObjectManagerReady)
		((LoggingComponentManager*)LoggingComponentManager::Get())->SignalNewLogMessage(Logging::MessageType::None, Message, tags);
}

void Logging::Context::AppendWithValue(const std::string_view& Message, const Reflection::GenericValue& Value, const std::string_view& ExtraTags) const
{
	ZoneScoped;

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
			.Type = Logging::MessageType::None
		});
		return;
	}

	//if (ExtraTags != "")
	//	appendToLog(ExtraTags, true);
	appendToLog(Message, false);

	if (s_IsGameObjectManagerReady)
		((LoggingComponentManager*)LoggingComponentManager::Get())->SignalNewLogMessage(Logging::MessageType::None, Message, tags, Value);
}

void Logging::Context::Info(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;

	std::string tags = std::string(ExtraTags);
	if (ContextExtraTags.size() > 0)
		tags += (tags.size() > 0 ? "," : "") + ContextExtraTags;

	if (std::this_thread::get_id() != MainThreadId)
	{
		std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelLogEventsMutex);
		ParallelLogEvents.push_back(ParallelLogEvent{
			.Message = std::string(Message),
			.ExtraTags = tags,
			.Type = Logging::MessageType::Info
		});
		return;
	}

	appendToLog("[INFO]", true);
	//appendToLog(ExtraTags, true);
	appendToLog(": ", true);
	appendToLog(Message);

	if (!s_IsGameObjectManagerReady && Message == "GameObjectManagerReady")
		s_IsGameObjectManagerReady = true;
	else if (s_IsGameObjectManagerReady && Message == "GameObjectManagerDead")
		s_IsGameObjectManagerReady = false;

	if (s_IsGameObjectManagerReady)
		((LoggingComponentManager*)LoggingComponentManager::Get())->SignalNewLogMessage(Logging::MessageType::Info, Message, tags);
}

void Logging::Context::Warning(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;

	std::string tags = std::string(ExtraTags);
	if (ContextExtraTags.size() > 0)
		tags += (tags.size() > 0 ? "," : "") + ContextExtraTags;

	if (std::this_thread::get_id() != MainThreadId)
	{
		std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelLogEventsMutex);
		ParallelLogEvents.push_back(ParallelLogEvent{
			.Message = std::string(Message),
			.ExtraTags = tags,
			.Type = Logging::MessageType::Warning
		});
		return;
	}

	appendToLog("[WARN]", true);
	//appendToLog(ExtraTags, true);
	appendToLog(": ", true);
	appendToLog(Message);

	if (s_IsGameObjectManagerReady)
		((LoggingComponentManager*)LoggingComponentManager::Get())->SignalNewLogMessage(Logging::MessageType::Warning, Message, tags);
}

void Logging::Context::Error(const std::string_view& Message, const std::string_view& ExtraTags) const
{
	ZoneScoped;

	std::string tags = std::string(ExtraTags);
	if (ContextExtraTags.size() > 0)
		tags += (tags.size() > 0 ? "," : "") + ContextExtraTags;

	if (std::this_thread::get_id() != MainThreadId)
	{
		std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(ParallelLogEventsMutex);
		ParallelLogEvents.push_back(ParallelLogEvent{
			.Message = std::string(Message),
			.ExtraTags = tags,
			.Type = Logging::MessageType::Error
		});
		return;
	}

	appendToLog("[ERRR]", true);
	//appendToLog(ExtraTags, true);
	appendToLog(": ", true);
	appendToLog(Message);

	if (s_IsGameObjectManagerReady)
		((LoggingComponentManager*)LoggingComponentManager::Get())->SignalNewLogMessage(Logging::MessageType::Error, Message, tags);
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
