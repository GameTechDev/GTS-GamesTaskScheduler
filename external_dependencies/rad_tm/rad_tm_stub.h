#ifndef _RAD_TM_
#define _RAD_TM_

/*
   Copyright (C) 2009-2017, RAD Game Tools, Inc.
   Telemetry is a registered trademark of RAD Game Tools
   Phone: 425-893-4300
   Sales: sales3@radgametools.com
   Support: telemetry@radgametools.com
   http://www.radgametools.com
   http://www.radgametools.com/telemetry.htm
   http://www.radgametools.com/telemetry/changelog.htm
*/

#define TM_SDK_MAJOR_VERSION 3
#define TM_SDK_MINOR_VERSION 0
#define TM_SDK_REVISION_NUMBER 1
#define TM_SDK_BUILD_NUMBER 1037
#define TM_SDK_VERSION "3.0.1.1037"
#define TmBuildVersion 3.0.1.1037
#define TM_BUILD_YEAR 2017
#define TM_BUILD_MONTH 5
#define TM_BUILD_DAY 31
#define TM_BUILD_SECOND 53662
#define TM_BUILD_DATE "2017.05.31.53662"
#define TmBuildDate 2017.05.31.53662

#define TM_API_MAJOR_VERSION 2017
#define TM_API_MINOR_VERSION 2
#define TM_API_REVISION_NUMBER 8
#define TM_API_BUILD_NUMBER 67529
#define TM_API_VERSION "2017.02.08.67529"


/* Typedefs */
#ifdef _WIN32
	typedef char* va_list;
	typedef __int64 tm_int64;
	typedef unsigned __int64 tm_uint64;
	typedef __declspec(align(8)) tm_uint64 tm_aligned_uint64;
#else // !_WIN32
	#include <stdarg.h> // for va_list
	typedef long long tm_int64;
	typedef unsigned long long tm_uint64;
	typedef tm_uint64 __attribute__ ((aligned (8))) tm_aligned_uint64;
#endif
typedef unsigned short tm_uint16;
typedef unsigned int tm_uint32;
typedef int tm_int32;
typedef tm_uint64 tm_string;
typedef void* (*tm_file_open_callback_type)(const char* filename, void* user_data);
typedef tm_uint32 (*tm_file_write_callback_type)(void* file_handle, void* data, tm_uint32 data_size, void* user_data);
typedef void (*tm_file_close_callback_type)(void* file_handle, void* user_data);

/* Enums */
typedef enum tm_error
{
	TM_OK = 0,							/* No error */
	TMERR_DISABLED,						/* Telemetry has been compiled away with NTELEMETRY */
	TMERR_INVALID_PARAM,				/* Out of range, null pointer, etc. */
	TMERR_NULL_API,						/* The Telemetry API is NULL so all Telemetry calls will no-op. Usually this means the telemetry DLL was not in your programs path. */
	TMERR_OUT_OF_RESOURCES,				/* Typically out of available memory, string space, etc. */
	TMERR_UNINITIALIZED,				/* A Telemetry API was called before tmInitialize */
	TMERR_BAD_HOSTNAME,					/* Could not resolve hostname */
	TMERR_COULD_NOT_CONNECT,			/* Could not connect to the server */
	TMERR_UNKNOWN_NETWORK,				/* Unknown error in the networking system */
	TMERR_ALREADY_SHUTDOWN,				/* tmShutdown called more than once */
	TMERR_ARENA_TOO_SMALL,				/* buffer passed to tmInitialize was too small */
	TMERR_BAD_HANDSHAKE,				/* handshake with server failed (protocol error) */
	TMERR_UNALIGNED,					/* One more more parameters were not aligned correctly */
	TMERR_NETWORK_NOT_INITIALIZED,		/* Network startup functions were not called, e.g. WSAStartup */
	TMERR_BAD_VERSION,					/* You're using an out of date version of the Telemetry libraries */
	TMERR_BAD_TIMER,					/* The provided user timer is too coarse for Telemetry */
	TMERR_ALREADY_OPENED,				/* Telemetry is already connected (tmOpen was already called). Call tmClose to close the existing connection before calling tmOpen. */
	TMERR_ALREADY_INITIALIZED,			/* tmInitialize was already called. tmInitialized only needs to be called once. */
	TMERR_FILE_OPEN_FAILED,				/* Telemetry couldn't open the file. */
	TMERR_INIT_NETWORKING_FAILED,		/* tmOpen was called with TMOF_INIT_NETWORKING, and that initialization failed. */
	TMERR_UNKNOWN = 0xFFFF,				/* Unknown error occurred */
} tm_error;

typedef enum tm_compression
{
	TMCOMPRESS_LZB = 0,			/* LZB compression */
	TMCOMPRESS_LZ4,				/* LZ4 compression */
	TMCOMPRESS_NONE,			/* No compression */
	TMCOMPRESS_UNKNOWN = 0x7FFFFFFF
} tm_compression;

typedef enum tm_zone_flags
{
	TMZF_NONE = 0,							/* Normal zones are drawn without any special color or dimensions */
	TMZF_STALL = 1 << 1,					/* Stall zones are drawn in red */
	TMZF_IDLE = 1 << 2,						/* Idle zones are drawn in grey. */
	TMZF_CONTEXT_SWITCH  = 1 << 3,			/* Context switch zones are drawn narrower */
	TMZF_PLOT_TIME  = 1 << 4,				/* Plot this zones time */
	TMZF_PLOT_TIME_EXPERIMENTAL = TMZF_PLOT_TIME, /* For compatibility with Telemetry 2 */
	TMZF_HAS_ACCUMULATION  = 1 << 5,		/* (Internal: do not use manually) This zone contains accumulations zones. */
	TMZF_ACCUMULATION  = 1 << 6,			/* (Internal: do not use manually) This zone represents an accumulation. */
	TMZF_LOCK_HOLD  = 1 << 7,				/* (Internal: do not use manually) This zone represents a time when a lock is held. */
	TMZF_LOCK_WAIT  = 1 << 8,				/* (Internal: do not use manually) This zone represents a time while waiting on a lock. */
} tm_zone_flags;

typedef enum tm_message_flags
{
	TMMF_SEVERITY_LOG = 0x0001,				/* Message is a standard log message */
	TMMF_SEVERITY_WARNING = 0x0002,			/* Message is a warning (drawn in yellow) */
	TMMF_SEVERITY_ERROR = 0x0004,			/* Message is an error (drawn in red) */
	TMMF_SEVERITY_RESERVED = 0x0008,		/* Unused */
	TMMF_SEVERITY_MASK = 0xf,				/* Low 3-bits determine the message type */
	TMMF_ZONE_LABEL = 0x0010,				/* Replace zone label with this text */
	TMMF_ZONE_SUBLABEL = 0x0020,			/* Print secondary zone label with text */
	TMMF_ZONE_SHOW_IN_PARENTS = 0x0040,		/* Show this not only in the zone's tooltip, but also in the tooltips of any parents */
	TMMF_ZONE_RESERVED01 = 0x0080,			/* Reserved */
	TMMF_ZONE_MASK = 0x00F0,				/* Mask for all of the zone values */
	TMMF_ICON_EXCLAMATION = 0x1000,			/* Show exclamation marker in the zone view for this message */
	TMMF_ICON_NOTE = 0x2000,				/* Show note marker in the zone view for this message */
	TMMF_ICON_QUESTION_MARK = 0x3000,		/* Show question mark (?) marker in the zone view for this message */
	TMMF_ICON_WTF = TMMF_ICON_QUESTION_MARK,/* Show question mark (?) marker in the zone view for this message */
	TMMF_ICON_EXTRA00 = 0x4000,				/* These are placeholders for now until we get some nicer icons */
	TMMF_ICON_EXTRA01 = 0x5000,				/* These are placeholders for now until we get some nicer icons */
	TMMF_ICON_EXTRA02 = 0x6000,				/* These are placeholders for now until we get some nicer icons */
	TMMF_ICON_EXTRA03 = 0x7000,				/* These are placeholders for now until we get some nicer icons */
	TMMF_ICON_EXTRA04 = 0x8000,				/* These are placeholders for now until we get some nicer icons */
	TMMF_ICON_MASK = 0xf000,				/* Mask for all of the icon values */
	TMMF_CALLSTACK = 0x10000,				/* Include the callstack in the message */
	TMMF_UNUSED01 = 0x20000					/* Unused */
} tm_message_flags;

typedef enum tm_library_type
{
	TM_RELEASE = 0,							/* Load the release version of the Telemetry library */
	TM_DEBUG								/* Load the debug version of the Telemetry library */
} tm_library_type;

typedef enum tm_open_flags
{
	TMOF_INIT_NETWORKING = 1 << 0,				/* Initialize operating system networking layer. Specify this if you do not already call the platform specific network startup functions (e.g. WSAStartup) prior to tmOpen.*/
	TMOF_CAPTURE_CONTEXT_SWITCHES = 1 << 1,		/* Enable capturing of context switches on platforms that support it. */
	TMOF_SAVE_PLOTS_CSV = 1 << 3,				/* Saves all plots to .CSV files when capture is finished. */
	TMOF_SAVE_ZONES_CSV = 1 << 4,				/* Saves all zones to a .CSV file when capture is finished. */
	TMOF_SAVE_TIME_SPANS_CSV = 1 << 5,			/* Saves all time spans to a .CSV file when capture is finished. */
	TMOF_SAVE_MESSAGES_CSV = 1 << 6,			/* Saves all messages to a .CSV file when capture is finished. */
	TMOF_GENERATE_HTML_REPORT = 1 << 7,			/* Generates an HTML report when the capture is finished. */
	TMOF_BREAK_ON_MISMATCHED_ZONE = 1 << 8,		/* Breaks when a zone underflow or overflow is detected. */
	TMOF_SAVE_ALL_CSV = TMOF_SAVE_PLOTS_CSV | TMOF_SAVE_ZONES_CSV | TMOF_SAVE_TIME_SPANS_CSV | TMOF_SAVE_MESSAGES_CSV /* Saves everything to to .CSV files when capture is finished. */
} tm_open_flags;

typedef enum tm_connection_type
{
	TMCT_TCP,								/* Standard network connection over TCP Socket */
	TMCT_IPC,								/* Reserved for future use. */
	TMCT_FILE,								/* Captures the TCP Socket stream directly to a file */
	TMCT_USER_PROVIDED,						/* Reserved for future use. */
} tm_connection_type;

typedef enum tm_plot_draw_type
{
	TM_PLOT_DRAW_LINE,						/* Draw plot as a line. */
	TM_PLOT_DRAW_POINTS,					/* Draw plot as points. */
	TM_PLOT_DRAW_STEPPED_XY,				/* Draw plot as a stair step moving horizontally and then vertically. */
	TM_PLOT_DRAW_STEPPED_YX,				/* Draw plot as a stair step moving vertically and then horizontally  */
	TM_PLOT_DRAW_COLUMN,					/* Draw plot as a column graph (AKA bar graph). */
	TM_PLOT_DRAW_LOLLIPOP					/* Draw plot as a lollipop graph. */
}tm_plot_draw_type;

typedef enum tm_plot_units
{
	TM_PLOT_UNITS_REAL = 0,					/* Plot value is a real number. */
	TM_PLOT_UNITS_MEMORY = 1,				/* Plot value is a memory value (in bytes) */
	TM_PLOT_UNITS_HEX = 2,					/* Plot value is shows in hexadecimal notation */
	TM_PLOT_UNITS_INTEGER = 3,				/* Plot value is an integer number */
	TM_PLOT_UNITS_PERCENTAGE_COMPUTED = 4,	/* Display as a percentage of the max, i.e. as (value-min)/(max-min), computed by the client */
	TM_PLOT_UNITS_PERCENTAGE_DIRECT = 5,	/* Display as a percentage (i.e. 0.2187 => 21.87%) */
	TM_PLOT_UNITS_TIME = 6,					/* Plot value is in seconds */
	TM_PLOT_UNITS_TIME_MS = 7,				/* Plot value is in milliseconds */
	TM_PLOT_UNITS_TIME_US = 8,				/* Plot value is in microseconds */
	TM_PLOT_UNITS_TIME_CLOCKS = 9,			/* Plot value is in CPU clocks */
	TM_PLOT_UNITS_TIME_INTERVAL = 10,		/* Plot value is in CPU clocks */
} tm_plot_units;

#define tmLoadLibrary(...)
#define tmCheckVersion(...) 0
#define tmInitialize(...) 0
#define tmOpen(...) TMERR_DISABLED
#define tmClose(...)
#define tmShutdown(...)
#define tmGetThreadHandle(...) 0
#define tmTick(...)
#define tmRegisterFileCallbacks(...)
#define tmFlush(...)
#define tmString(...) 0
#define tmStaticString(...) 0
#define tmPrintf(...) 0
#define tmPrintfV(...) 0
#define tmCallStack(...) 0
#define tmZoneColor(...)
#define tmZoneWaitingFor(...)
#define tmSetZoneFlag(...)
#define tmSetCaptureMask(...)
#define tmGetCaptureMask(...) 0
#define tmGetSessionName(...)
#define tmSetPlotInfo(...)
#define tmThreadName(...)
#define tmEndThread(...)
#define tmTrackOrder(...)
#define tmTrackColor(...)
#define tmHash(...) 0
#define tmHashString(...) 0
#define tmHash32(...) 0
#define tmSetMaxThreadCount(...)
#define tmGetMaxThreadCount(...) 0
#define tmSetMaxTimeSpanTrackCount(...)
#define tmGetMaxTimeSpanTrackCount(...) 0
#define tmSetSendBufferSize(...)
#define tmGetSendBufferSize(...) 0
#define tmSetSendBufferCount(...)
#define tmGetSendBufferCount(...) 0
#define tmGetMemoryFootprint(...) 0
#define tmRunning(...) 0
#define tmSetCompression(...)
#define tmSetZoneFilterThreshold(...)
#define tmNewTrackID(...) 0
#define tmTrackName(...)
#define tmThreadTrack(...) 0
#define tmStartWaitForLock(...)
#define tmStartWaitForLockBase(...)
#define tmStartWaitForLock1(...)
#define tmStartWaitForLock2(...)
#define tmStartWaitForLock3(...)
#define tmStartWaitForLock4(...)
#define tmStartWaitForLock5(...)
#define tmStartWaitForLockEx(...)
#define tmStartWaitForLockExBase(...)
#define tmStartWaitForLockEx1(...)
#define tmStartWaitForLockEx2(...)
#define tmStartWaitForLockEx3(...)
#define tmStartWaitForLockEx4(...)
#define tmStartWaitForLockEx5(...)
#define tmStartWaitForLockManual(...)
#define tmStartWaitForLockManualBase(...)
#define tmStartWaitForLockManual1(...)
#define tmStartWaitForLockManual2(...)
#define tmStartWaitForLockManual3(...)
#define tmStartWaitForLockManual4(...)
#define tmStartWaitForLockManual5(...)
#define tmAcquiredLock(...)
#define tmAcquiredLockBase(...)
#define tmAcquiredLock1(...)
#define tmAcquiredLock2(...)
#define tmAcquiredLock3(...)
#define tmAcquiredLock4(...)
#define tmAcquiredLock5(...)
#define tmAcquiredLockEx(...)
#define tmAcquiredLockExBase(...)
#define tmAcquiredLockEx1(...)
#define tmAcquiredLockEx2(...)
#define tmAcquiredLockEx3(...)
#define tmAcquiredLockEx4(...)
#define tmAcquiredLockEx5(...)
#define tmAcquiredLockManual(...)
#define tmAcquiredLockManualBase(...)
#define tmAcquiredLockManual1(...)
#define tmAcquiredLockManual2(...)
#define tmAcquiredLockManual3(...)
#define tmAcquiredLockManual4(...)
#define tmAcquiredLockManual5(...)
#define tmReleasedLock(...)
#define tmReleasedLockBase(...)
#define tmReleasedLock1(...)
#define tmReleasedLock2(...)
#define tmReleasedLock3(...)
#define tmReleasedLock4(...)
#define tmReleasedLock5(...)
#define tmReleasedLockEx(...)
#define tmReleasedLockExBase(...)
#define tmReleasedLockEx1(...)
#define tmReleasedLockEx2(...)
#define tmReleasedLockEx3(...)
#define tmReleasedLockEx4(...)
#define tmReleasedLockEx5(...)
#define tmEndWaitForLock(...)
#define tmEndWaitForLockBase(...)
#define tmEndWaitForLock1(...)
#define tmEndWaitForLock2(...)
#define tmEndWaitForLock3(...)
#define tmEndWaitForLock4(...)
#define tmEndWaitForLock5(...)
#define tmMessage(...)
#define tmMessageBase(...)
#define tmMessage1(...)
#define tmMessage2(...)
#define tmMessage3(...)
#define tmMessage4(...)
#define tmMessage5(...)
#define tmMessageEx(...)
#define tmMessageExBase(...)
#define tmMessageEx1(...)
#define tmMessageEx2(...)
#define tmMessageEx3(...)
#define tmMessageEx4(...)
#define tmMessageEx5(...)
#define tmPlot(...)
#define tmPlotBase(...)
#define tmPlot1(...)
#define tmPlot2(...)
#define tmPlot3(...)
#define tmPlot4(...)
#define tmPlot5(...)
#define tmPlotAt(...)
#define tmPlotAtBase(...)
#define tmPlotAt1(...)
#define tmPlotAt2(...)
#define tmPlotAt3(...)
#define tmPlotAt4(...)
#define tmPlotAt5(...)
#define tmZone(...)
#define tmZoneBase(...)
#define tmZone1(...)
#define tmZone2(...)
#define tmZone3(...)
#define tmZone4(...)
#define tmZone5(...)
#define tmZoneEx(...)
#define tmZoneExBase(...)
#define tmZoneEx1(...)
#define tmZoneEx2(...)
#define tmZoneEx3(...)
#define tmZoneEx4(...)
#define tmZoneEx5(...)
#define tmFunction(...)
#define tmFunctionBase(...)
#define tmFunction1(...)
#define tmFunction2(...)
#define tmFunction3(...)
#define tmFunction4(...)
#define tmFunction5(...)
#define tmEnterAccumulationZone(...)
#define tmEnterAccumulationZoneBase(...)
#define tmEnterAccumulationZone1(...)
#define tmEnterAccumulationZone2(...)
#define tmEnterAccumulationZone3(...)
#define tmEnterAccumulationZone4(...)
#define tmEnterAccumulationZone5(...)
#define tmLeaveAccumulationZone(...)
#define tmLeaveAccumulationZoneBase(...)
#define tmLeaveAccumulationZone1(...)
#define tmLeaveAccumulationZone2(...)
#define tmLeaveAccumulationZone3(...)
#define tmLeaveAccumulationZone4(...)
#define tmLeaveAccumulationZone5(...)
#define tmEmitAccumulationZone(...)
#define tmEmitAccumulationZoneBase(...)
#define tmEmitAccumulationZone1(...)
#define tmEmitAccumulationZone2(...)
#define tmEmitAccumulationZone3(...)
#define tmEmitAccumulationZone4(...)
#define tmEmitAccumulationZone5(...)
#define tmBeginTimeSpan(...)
#define tmBeginTimeSpanBase(...)
#define tmBeginTimeSpan1(...)
#define tmBeginTimeSpan2(...)
#define tmBeginTimeSpan3(...)
#define tmBeginTimeSpan4(...)
#define tmBeginTimeSpan5(...)
#define tmBeginTimeSpanEx(...)
#define tmBeginTimeSpanExBase(...)
#define tmBeginTimeSpanEx1(...)
#define tmBeginTimeSpanEx2(...)
#define tmBeginTimeSpanEx3(...)
#define tmBeginTimeSpanEx4(...)
#define tmBeginTimeSpanEx5(...)
#define tmEndTimeSpan(...)
#define tmEndTimeSpanBase(...)
#define tmEndTimeSpan1(...)
#define tmEndTimeSpan2(...)
#define tmEndTimeSpan3(...)
#define tmEndTimeSpan4(...)
#define tmEndTimeSpan5(...)
#define tmEndTimeSpanEx(...)
#define tmEndTimeSpanExBase(...)
#define tmEndTimeSpanEx1(...)
#define tmEndTimeSpanEx2(...)
#define tmEndTimeSpanEx3(...)
#define tmEndTimeSpanEx4(...)
#define tmEndTimeSpanEx5(...)
#define tmSetTimeSpanName(...)
#define tmSetTimeSpanNameBase(...)
#define tmSetTimeSpanName1(...)
#define tmSetTimeSpanName2(...)
#define tmSetTimeSpanName3(...)
#define tmSetTimeSpanName4(...)
#define tmSetTimeSpanName5(...)
#define tmEnter(...)
#define tmEnterBase(...)
#define tmEnter1(...)
#define tmEnter2(...)
#define tmEnter3(...)
#define tmEnter4(...)
#define tmEnter5(...)
#define tmEnterEx(...)
#define tmEnterExBase(...)
#define tmEnterEx1(...)
#define tmEnterEx2(...)
#define tmEnterEx3(...)
#define tmEnterEx4(...)
#define tmEnterEx5(...)
#define tmLeave(...)
#define tmLeaveBase(...)
#define tmLeave1(...)
#define tmLeave2(...)
#define tmLeave3(...)
#define tmLeave4(...)
#define tmLeave5(...)
#define tmLeaveEx(...)
#define tmLeaveExBase(...)
#define tmLeaveEx1(...)
#define tmLeaveEx2(...)
#define tmLeaveEx3(...)
#define tmLeaveEx4(...)
#define tmLeaveEx5(...)
#define tmRenameZone(...)
#define tmRenameZoneBase(...)
#define tmRenameZone1(...)
#define tmRenameZone2(...)
#define tmRenameZone3(...)
#define tmRenameZone4(...)
#define tmRenameZone5(...)
#define tmZoneAtTime(...)
#define tmZoneAtTimeBase(...)
#define tmZoneAtTime1(...)
#define tmZoneAtTime2(...)
#define tmZoneAtTime3(...)
#define tmZoneAtTime4(...)
#define tmZoneAtTime5(...)
#define tmAlloc(...)
#define tmAllocBase(...)
#define tmAlloc1(...)
#define tmAlloc2(...)
#define tmAlloc3(...)
#define tmAlloc4(...)
#define tmAlloc5(...)
#define tmAllocEx(...)
#define tmAllocExBase(...)
#define tmAllocEx1(...)
#define tmAllocEx2(...)
#define tmAllocEx3(...)
#define tmAllocEx4(...)
#define tmAllocEx5(...)
#define tmFree(...)
#define tmFreeBase(...)
#define tmFree1(...)
#define tmFree2(...)
#define tmFree3(...)
#define tmFree4(...)
#define tmFree5(...)
#define tmFreeEx(...)
#define tmFreeExBase(...)
#define tmFreeEx1(...)
#define tmFreeEx2(...)
#define tmFreeEx3(...)
#define tmFreeEx4(...)
#define tmFreeEx5(...)

#endif /* _RAD_TM_ */

