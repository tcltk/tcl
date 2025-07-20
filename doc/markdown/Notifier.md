---
CommandName: Notifier
ManualSection: 3
Version: 8.1
TclPart: Tcl
TclDescription: Tcl Library Procedures
Links:
 - Tcl_CreateFileHandler(3)
 - Tcl_DeleteFileHandler(3)
 - Tcl_Sleep(3)
 - Tcl_DoOneEvent(3)
 - Thread(3)
Keywords:
 - event
 - notifier
 - event queue
 - event sources
 - file events
 - timer
 - idle
 - service mode
 - threads
Copyright:
 - Copyright (c) 1998-1999 Scriptics Corporation
 - Copyright (c) 1995-1997 Sun Microsystems, Inc.
---

# Name

Tcl_CreateEventSource, Tcl_DeleteEventSource, Tcl_SetMaxBlockTime, Tcl_QueueEvent, Tcl_ThreadQueueEvent, Tcl_ThreadAlert, Tcl_GetCurrentThread, Tcl_DeleteEvents, Tcl_InitNotifier, Tcl_FinalizeNotifier, Tcl_WaitForEvent, Tcl_AlertNotifier, Tcl_SetTimer, Tcl_ServiceAll, Tcl_ServiceEvent, Tcl_GetServiceMode, Tcl_SetServiceMode, Tcl_ServiceModeHook, Tcl_SetNotifier - the event queue and notifier interfaces

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl_CreateEventSource]{.ccmd}[setupProc, checkProc, clientData]{.cargs}
[Tcl_DeleteEventSource]{.ccmd}[setupProc, checkProc, clientData]{.cargs}
[Tcl_SetMaxBlockTime]{.ccmd}[timePtr]{.cargs}
[Tcl_QueueEvent]{.ccmd}[evPtr, position]{.cargs}
[Tcl_ThreadQueueEvent]{.ccmd}[threadId, evPtr, position]{.cargs}
[Tcl_ThreadAlert]{.ccmd}[threadId]{.cargs}
[Tcl_ThreadId]{.ret} [Tcl_GetCurrentThread]{.ccmd}[]{.cargs}
[Tcl_DeleteEvents]{.ccmd}[deleteProc, clientData]{.cargs}
[void *]{.ret} [Tcl_InitNotifier]{.ccmd}[]{.cargs}
[Tcl_FinalizeNotifier]{.ccmd}[clientData]{.cargs}
[int]{.ret} [Tcl_WaitForEvent]{.ccmd}[timePtr]{.cargs}
[Tcl_AlertNotifier]{.ccmd}[clientData]{.cargs}
[Tcl_SetTimer]{.ccmd}[timePtr]{.cargs}
[int]{.ret} [Tcl_ServiceAll]{.ccmd}[]{.cargs}
[int]{.ret} [Tcl_ServiceEvent]{.ccmd}[flags]{.cargs}
[int]{.ret} [Tcl_GetServiceMode]{.ccmd}[]{.cargs}
[int]{.ret} [Tcl_SetServiceMode]{.ccmd}[mode]{.cargs}
[Tcl_ServiceModeHook]{.ccmd}[mode]{.cargs}
[Tcl_SetNotifier]{.ccmd}[notifierProcPtr]{.cargs}
:::

# Arguments

.AP Tcl_EventSetupProc *setupProc in Procedure to invoke to prepare for event wait in **Tcl_DoOneEvent**. .AP Tcl_EventCheckProc *checkProc in Procedure for **Tcl_DoOneEvent** to invoke after waiting for events.  Checks to see if any events have occurred and, if so, queues them. .AP void *clientData in Arbitrary one-word value to pass to *setupProc*, *checkProc*, or *deleteProc*. .AP "const Tcl_Time" *timePtr in Indicates the maximum amount of time to wait for an event.  This is specified as an interval (how long to wait), not an absolute time (when to wakeup).  If the pointer passed to **Tcl_WaitForEvent** is NULL, it means there is no maximum wait time:  wait forever if necessary. .AP Tcl_Event *evPtr in An event to add to the event queue.  The storage for the event must have been allocated by the caller using **Tcl_Alloc**. .AP int position in Where to add the new event in the queue:  **TCL_QUEUE_TAIL**, **TCL_QUEUE_HEAD**, **TCL_QUEUE_MARK**, and whether to do an alert if the queue is empty: **TCL_QUEUE_ALERT_IF_EMPTY**. .AP Tcl_ThreadId threadId in A unique identifier for a thread. .AP Tcl_EventDeleteProc *deleteProc in Procedure to invoke for each queued event in **Tcl_DeleteEvents**. .AP int flags in What types of events to service.  These flags are the same as those passed to **Tcl_DoOneEvent**. .AP int mode in Indicates whether events should be serviced by **Tcl_ServiceAll**. Must be one of **TCL_SERVICE_NONE** or **TCL_SERVICE_ALL**. .AP const Tcl_NotifierProcs* notifierProcPtr in Structure of function pointers describing notifier procedures that are to replace the ones installed in the executable.  See **REPLACING THE NOTIFIER** for details.

# Introduction

The interfaces described here are used to customize the Tcl event loop.  The two most common customizations are to add new sources of events and to merge Tcl's event loop with some other event loop, such as one provided by an application in which Tcl is embedded.  Each of these tasks is described in a separate section below.

The procedures in this manual entry are the building blocks out of which the Tcl event notifier is constructed.  The event notifier is the lowest layer in the Tcl event mechanism.  It consists of three things:

1. Event sources: these represent the ways in which events can be generated.  For example, there is a timer event source that implements the **Tcl_CreateTimerHandler** procedure and the **after** command, and there is a file event source that implements the **Tcl_CreateFileHandler** procedure on Unix systems.  An event source must work with the notifier to detect events at the right times, record them on the event queue, and eventually notify higher-level software that they have occurred.  The procedures **Tcl_CreateEventSource**, **Tcl_DeleteEventSource**, and **Tcl_SetMaxBlockTime**, **Tcl_QueueEvent**, and **Tcl_DeleteEvents** are used primarily by event sources.

2. The event queue: there is a single queue for each thread containing a Tcl interpreter, containing events that have been detected but not yet serviced.  Event sources place events onto the queue so that they may be processed in order at appropriate times during the event loop. The event queue guarantees a fair discipline of event handling, so that no event source can starve the others.  It also allows events to be saved for servicing at a future time. **Tcl_QueueEvent** is used (primarily by event sources) to add events to the current thread's event queue and **Tcl_DeleteEvents** is used to remove events from the queue without processing them.

3. The event loop: in order to detect and process events, the application enters a loop that waits for events to occur, places them on the event queue, and then processes them.  Most applications will do this by calling the procedure **Tcl_DoOneEvent**, which is described in a separate manual entry.


Most Tcl applications need not worry about any of the internals of the Tcl notifier.  However, the notifier now has enough flexibility to be retargeted either for a new platform or to use an external event loop (such as the Motif event loop, when Tcl is embedded in a Motif application).  The procedures **Tcl_WaitForEvent** and **Tcl_SetTimer** are normally implemented by Tcl, but may be replaced with new versions to retarget the notifier (the **Tcl_InitNotifier**, **Tcl_AlertNotifier**, **Tcl_FinalizeNotifier**, **Tcl_Sleep**, **Tcl_CreateFileHandler**, and **Tcl_DeleteFileHandler** must also be replaced; see CREATING A NEW NOTIFIER below for details). The procedures **Tcl_ServiceAll**, **Tcl_ServiceEvent**, **Tcl_GetServiceMode**, and **Tcl_SetServiceMode** are provided to help connect Tcl's event loop to an external event loop such as Motif's.

# Notifier basics

The easiest way to understand how the notifier works is to consider what happens when **Tcl_DoOneEvent** is called. **Tcl_DoOneEvent** is passed a *flags* argument that indicates what sort of events it is OK to process and also whether or not to block if no events are ready.  **Tcl_DoOneEvent** does the following things:

1. Check the event queue to see if it contains any events that can be serviced.  If so, service the first possible event, remove it from the queue, and return.  It does this by calling **Tcl_ServiceEvent** and passing in the *flags* argument.

2. Prepare to block for an event.  To do this, **Tcl_DoOneEvent** invokes a *setup procedure* in each event source. The event source will perform event-source specific initialization and possibly call **Tcl_SetMaxBlockTime** to limit how long **Tcl_WaitForEvent** will block if no new events occur.

3. Call **Tcl_WaitForEvent**.  This procedure is implemented differently on different platforms;  it waits for an event to occur, based on the information provided by the event sources. It may cause the application to block if *timePtr* specifies an interval other than 0. **Tcl_WaitForEvent** returns when something has happened, such as a file becoming readable or the interval given by *timePtr* expiring.  If there are no events for **Tcl_WaitForEvent** to wait for, so that it would block forever, then it returns immediately and **Tcl_DoOneEvent** returns 0.

4. Call a *check procedure* in each event source.  The check procedure determines whether any events of interest to this source occurred.  If so, the events are added to the event queue.

5. Check the event queue to see if it contains any events that can be serviced.  If so, service the first possible event, remove it from the queue, and return.

6. See if there are idle callbacks pending. If so, invoke all of them and return.

7. Either return 0 to indicate that no events were ready, or go back to step [2] if blocking was requested by the caller.


# Creating a new event source

An event source consists of three procedures invoked by the notifier, plus additional C procedures that are invoked by higher-level code to arrange for event-driven callbacks.  The three procedures called by the notifier consist of the setup and check procedures described above, plus an additional procedure that is invoked when an event is removed from the event queue for servicing.

The procedure **Tcl_CreateEventSource** creates a new event source. Its arguments specify the setup procedure and check procedure for the event source. *SetupProc* should match the following prototype:

```
typedef void Tcl_EventSetupProc(
        void *clientData,
        int flags);
```

The *clientData* argument will be the same as the *clientData* argument to **Tcl_CreateEventSource**;  it is typically used to point to private information managed by the event source. The *flags* argument will be the same as the *flags* argument passed to **Tcl_DoOneEvent** except that it will never be 0 (**Tcl_DoOneEvent** replaces 0 with **TCL_ALL_EVENTS**). *Flags* indicates what kinds of events should be considered; if the bit corresponding to this event source is not set, the event source should return immediately without doing anything.  For example, the file event source checks for the **TCL_FILE_EVENTS** bit.

*SetupProc*'s job is to make sure that the application wakes up when events of the desired type occur.  This is typically done in a platform-dependent fashion.  For example, under Unix an event source might call **Tcl_CreateFileHandler**; under Windows it might request notification with a Windows event.  For timer-driven event sources such as timer events or any polled event, the event source can call **Tcl_SetMaxBlockTime** to force the application to wake up after a specified time even if no events have occurred. If no event source calls **Tcl_SetMaxBlockTime** then **Tcl_WaitForEvent** will wait as long as necessary for an event to occur; otherwise, it will only wait as long as the shortest interval passed to **Tcl_SetMaxBlockTime** by one of the event sources.  If an event source knows that it already has events ready to report, it can request a zero maximum block time.  For example, the setup procedure for the X event source looks to see if there are events already queued.  If there are, it calls **Tcl_SetMaxBlockTime** with a 0 block time so that **Tcl_WaitForEvent** does not block if there is no new data on the X connection. The *timePtr* argument to **Tcl_WaitForEvent** points to a structure that describes a time interval in seconds and microseconds:

```
typedef struct {
    long long sec;
    long usec;
} Tcl_Time;
```

The *usec* field should be less than 1000000.

Information provided to **Tcl_SetMaxBlockTime** is only used for the next call to **Tcl_WaitForEvent**; it is discarded after **Tcl_WaitForEvent** returns. The next time an event wait is done each of the event sources' setup procedures will be called again, and they can specify new information for that event wait.

If the application uses an external event loop rather than **Tcl_DoOneEvent**, the event sources may need to call **Tcl_SetMaxBlockTime** at other times.  For example, if a new event handler is registered that needs to poll for events, the event source may call **Tcl_SetMaxBlockTime** to set the block time to zero to force the external event loop to call Tcl.  In this case, **Tcl_SetMaxBlockTime** invokes **Tcl_SetTimer** with the shortest interval seen since the last call to **Tcl_DoOneEvent** or **Tcl_ServiceAll**.

In addition to the generic procedure **Tcl_SetMaxBlockTime**, other platform-specific procedures may also be available for *setupProc*, if there is additional information needed by **Tcl_WaitForEvent** on that platform.  For example, on Unix systems the **Tcl_CreateFileHandler** interface can be used to wait for file events.

The second procedure provided by each event source is its check procedure, indicated by the *checkProc* argument to **Tcl_CreateEventSource**.  *CheckProc* must match the following prototype:

```
typedef void Tcl_EventCheckProc(
        void *clientData,
        int flags);
```

The arguments to this procedure are the same as those for *setupProc*. **CheckProc** is invoked by **Tcl_DoOneEvent** after it has waited for events.  Presumably at least one event source is now prepared to queue an event.  **Tcl_DoOneEvent** calls each of the event sources in turn, so they all have a chance to queue any events that are ready. The check procedure does two things.  First, it must see if any events have triggered.  Different event sources do this in different ways.

If an event source's check procedure detects an interesting event, it must add the event to Tcl's event queue.  To do this, the event source calls **Tcl_QueueEvent**.  The *evPtr* argument is a pointer to a dynamically allocated structure containing the event (see below for more information on memory management issues).  Each event source can define its own event structure with whatever information is relevant to that event source.  However, the first element of the structure must be a structure of type **Tcl_Event**, and the address of this structure is used when communicating between the event source and the rest of the notifier.  A **Tcl_Event** has the following definition:

```
typedef struct Tcl_Event {
    Tcl_EventProc *proc;
    struct Tcl_Event *nextPtr;
} Tcl_Event;
```

The event source must fill in the *proc* field of the event before calling **Tcl_QueueEvent**. The *nextPtr* is used to link together the events in the queue and should not be modified by the event source.

An event may be added to the queue at any of three positions, depending on the *position* argument to **Tcl_QueueEvent**:

**TCL_QUEUE_TAIL**
: Add the event at the back of the queue, so that all other pending events will be serviced first.  This is almost always the right place for new events.

**TCL_QUEUE_HEAD**
: Add the event at the front of the queue, so that it will be serviced before all other queued events.

**TCL_QUEUE_MARK**
: Add the event at the front of the queue, unless there are other events at the front whose position is **TCL_QUEUE_MARK**;  if so, add the new event just after all other **TCL_QUEUE_MARK** events. This value of *position* is used to insert an ordered sequence of events at the front of the queue, such as a series of Enter and Leave events synthesized during a grab or ungrab operation in Tk.

**TCL_QUEUE_ALERT_IF_EMPTY**
: When used in **Tcl_ThreadQueueEvent** arranges for an automatic call of **Tcl_ThreadAlert** when the queue was empty.


When it is time to handle an event from the queue (steps 1 and 4 above) **Tcl_ServiceEvent** will invoke the *proc* specified in the first queued **Tcl_Event** structure. *Proc* must match the following prototype:

```
typedef int Tcl_EventProc(
        Tcl_Event *evPtr,
        int flags);
```

The first argument to *proc* is a pointer to the event, which will be the same as the first argument to the **Tcl_QueueEvent** call that added the event to the queue. The second argument to *proc* is the *flags* argument for the current call to **Tcl_ServiceEvent**;  this is used by the event source to return immediately if its events are not relevant.

It is up to *proc* to handle the event, typically by invoking one or more Tcl commands or C-level callbacks. Once the event source has finished handling the event it returns 1 to indicate that the event can be removed from the queue. If for some reason the event source decides that the event cannot be handled at this time, it may return 0 to indicate that the event should be deferred for processing later;  in this case **Tcl_ServiceEvent** will go on to the next event in the queue and attempt to service it. There are several reasons why an event source might defer an event. One possibility is that events of this type are excluded by the *flags* argument. For example, the file event source will always return 0 if the **TCL_FILE_EVENTS** bit is not set in *flags*. Another example of deferring events happens in Tk if **Tk_RestrictEvents** has been invoked to defer certain kinds of window events.

When *proc* returns 1, **Tcl_ServiceEvent** will remove the event from the event queue and free its storage. Note that the storage for an event must be allocated by the event source (using **Tcl_Alloc**) before calling **Tcl_QueueEvent**, but it will be freed by **Tcl_ServiceEvent**, not by the event source.

Calling **Tcl_QueueEvent** adds an event to the current thread's queue. To add an event to another thread's queue, use **Tcl_ThreadQueueEvent**. **Tcl_ThreadQueueEvent** accepts as an argument a Tcl_ThreadId argument, which uniquely identifies a thread in a Tcl application.  To obtain the Tcl_ThreadId for the current thread, use the **Tcl_GetCurrentThread** procedure.  (A thread would then need to pass this identifier to other threads for those threads to be able to add events to its queue.) After adding an event to another thread's queue, you then typically need to call **Tcl_ThreadAlert** to "wake up" that thread's notifier to alert it to the new event.

**Tcl_DeleteEvents** can be used to explicitly remove one or more events from the event queue.  **Tcl_DeleteEvents** calls *proc* for each event in the queue, deleting those for with the procedure returns 1.  Events for which the procedure returns 0 are left in the queue.  *Proc* should match the following prototype:

```
typedef int Tcl_EventDeleteProc(
        Tcl_Event *evPtr,
        void *clientData);
```

The *clientData* argument will be the same as the *clientData* argument to **Tcl_DeleteEvents**; it is typically used to point to private information managed by the event source.  The *evPtr* will point to the next event in the queue.

**Tcl_DeleteEventSource** deletes an event source.  The *setupProc*, *checkProc*, and *clientData* arguments must exactly match those provided to the **Tcl_CreateEventSource** for the event source to be deleted. If no such source exists, **Tcl_DeleteEventSource** has no effect.

# Creating a new notifier

The notifier consists of all the procedures described in this manual entry, plus **Tcl_DoOneEvent** and **Tcl_Sleep**, which are available on all platforms, and **Tcl_CreateFileHandler** and **Tcl_DeleteFileHandler**, which are Unix-specific.  Most of these procedures are generic, in that they are the same for all notifiers. However, none of the procedures are notifier-dependent: **Tcl_InitNotifier**, **Tcl_AlertNotifier**, **Tcl_FinalizeNotifier**, **Tcl_SetTimer**, **Tcl_Sleep**, **Tcl_WaitForEvent**, **Tcl_CreateFileHandler**, **Tcl_DeleteFileHandler** and **Tcl_ServiceModeHook**.  To support a new platform or to integrate Tcl with an application-specific event loop, you must write new versions of these procedures.

**Tcl_InitNotifier** initializes the notifier state and returns a handle to the notifier state.  Tcl calls this procedure when initializing a Tcl interpreter.  Similarly, **Tcl_FinalizeNotifier** shuts down the notifier, and is called by **Tcl_Finalize** when shutting down a Tcl interpreter.

**Tcl_WaitForEvent** is the lowest-level procedure in the notifier; it is responsible for waiting for an "interesting" event to occur or for a given time to elapse.  Before **Tcl_WaitForEvent** is invoked, each of the event sources' setup procedure will have been invoked. The *timePtr* argument to **Tcl_WaitForEvent** gives the maximum time to block for an event, based on calls to **Tcl_SetMaxBlockTime** made by setup procedures and on other information (such as the **TCL_DONT_WAIT** bit in *flags*).

Ideally, **Tcl_WaitForEvent** should only wait for an event to occur; it should not actually process the event in any way. Later on, the event sources will process the raw events and create Tcl_Events on the event queue in their *checkProc* procedures. However, on some platforms (such as Windows) this is not possible; events may be processed in **Tcl_WaitForEvent**, including queuing Tcl_Events and more (for example, callbacks for native widgets may be invoked).  The return value from **Tcl_WaitForEvent** must be either 0, 1, or -1.  On platforms such as Windows where events get processed in **Tcl_WaitForEvent**, a return value of 1 means that there may be more events still pending that have not been processed.  This is a sign to the caller that it must call **Tcl_WaitForEvent** again if it wants all pending events to be processed. A 0 return value means that calling **Tcl_WaitForEvent** again will not have any effect: either this is a platform where **Tcl_WaitForEvent** only waits without doing any event processing, or **Tcl_WaitForEvent** knows for sure that there are no additional events to process (e.g. it returned because the time elapsed).  Finally, a return value of -1 means that the event loop is no longer operational and the application should probably unwind and terminate.  Under Windows this happens when a WM_QUIT message is received; under Unix it happens when **Tcl_WaitForEvent** would have waited forever because there were no active event sources and the timeout was infinite.

**Tcl_AlertNotifier** is used to allow any thread to "wake up" the notifier to alert it to new events on its queue.  **Tcl_AlertNotifier** requires as an argument the notifier handle returned by **Tcl_InitNotifier**.

If the notifier will be used with an external event loop, then it must also support the **Tcl_SetTimer** interface.  **Tcl_SetTimer** is invoked by **Tcl_SetMaxBlockTime** whenever the maximum blocking time has been reduced.  **Tcl_SetTimer** should arrange for the external event loop to invoke **Tcl_ServiceAll** after the specified interval even if no events have occurred.  This interface is needed because **Tcl_WaitForEvent** is not invoked when there is an external event loop.  If the notifier will only be used from **Tcl_DoOneEvent**, then **Tcl_SetTimer** need not do anything.

**Tcl_ServiceModeHook** is called by the platform-independent portion of the notifier when client code makes a call to **Tcl_SetServiceMode**. This hook is provided to support operating systems that require special event handling when the application is in a modal loop (the Windows notifier, for instance, uses this hook to create a communication window).

On Unix systems, the file event source also needs support from the notifier.  The file event source consists of the **Tcl_CreateFileHandler** and **Tcl_DeleteFileHandler** procedures, which are described in the **Tcl_CreateFileHandler** manual page.

The **Tcl_Sleep** and **Tcl_DoOneEvent** interfaces are described in their respective manual pages.

The easiest way to create a new notifier is to look at the code for an existing notifier, such as the files **unix/tclUnixNotfy.c** or **win/tclWinNotify.c** in the Tcl source distribution.

# Replacing the notifier

A notifier that has been written according to the conventions above can also be installed in a running process in place of the standard notifier.  This mechanism is used so that a single executable can be used (with the standard notifier) as a stand-alone program and reused (with a replacement notifier in a loadable extension) as an extension to another program, such as a Web browser plugin.

To do this, the extension makes a call to **Tcl_SetNotifier** passing a pointer to a **Tcl_NotifierProcs** data structure.  The structure has the following layout:

```
typedef struct {
    Tcl_SetTimerProc *setTimerProc;
    Tcl_WaitForEventProc *waitForEventProc;
    Tcl_CreateFileHandlerProc *createFileHandlerProc;
    Tcl_DeleteFileHandlerProc *deleteFileHandlerProc;
    Tcl_InitNotifierProc *initNotifierProc;
    Tcl_FinalizeNotifierProc *finalizeNotifierProc;
    Tcl_AlertNotifierProc *alertNotifierProc;
    Tcl_ServiceModeHookProc *serviceModeHookProc;
} Tcl_NotifierProcs;
```

Following the call to **Tcl_SetNotifier**, the pointers given in the **Tcl_NotifierProcs** structure replace whatever notifier had been installed in the process.

It is extraordinarily unwise to replace a running notifier. Normally, **Tcl_SetNotifier** should be called at process initialization time before the first call to **Tcl_InitNotifier**.

# External event loops

The notifier interfaces are designed so that Tcl can be embedded into applications that have their own private event loops.  In this case, the application does not call **Tcl_DoOneEvent** except in the case of recursive event loops such as calls to the Tcl commands **update** or **vwait**.  Most of the time is spent in the external event loop of the application.  In this case the notifier must arrange for the external event loop to call back into Tcl when something happens on the various Tcl event sources.  These callbacks should arrange for appropriate Tcl events to be placed on the Tcl event queue.

Because the external event loop is not calling **Tcl_DoOneEvent** on a regular basis, it is up to the notifier to arrange for **Tcl_ServiceEvent** to be called whenever events are pending on the Tcl event queue.  The easiest way to do this is to invoke **Tcl_ServiceAll** at the end of each callback from the external event loop.  This will ensure that all of the event sources are polled, any queued events are serviced, and any pending idle handlers are processed before returning control to the application.  In addition, event sources that need to poll for events can call **Tcl_SetMaxBlockTime** to force the external event loop to call Tcl even if no events are available on the system event queue.

As a side effect of processing events detected in the main external event loop, Tcl may invoke **Tcl_DoOneEvent** to start a recursive event loop in commands like **vwait**.  **Tcl_DoOneEvent** will invoke the external event loop, which will result in callbacks as described in the preceding paragraph, which will result in calls to **Tcl_ServiceAll**.  However, in these cases it is undesirable to service events in **Tcl_ServiceAll**.  Servicing events there is unnecessary because control will immediately return to the external event loop and hence to **Tcl_DoOneEvent**, which can service the events itself.  Furthermore, **Tcl_DoOneEvent** is supposed to service only a single event, whereas **Tcl_ServiceAll** normally services all pending events.  To handle this situation, **Tcl_DoOneEvent** sets a flag for **Tcl_ServiceAll** that causes it to return without servicing any events. This flag is called the *service mode*; **Tcl_DoOneEvent** restores it to its previous value before it returns.

In some cases, however, it may be necessary for **Tcl_ServiceAll** to service events even when it has been invoked from **Tcl_DoOneEvent**.  This happens when there is yet another recursive event loop invoked via an event handler called by **Tcl_DoOneEvent** (such as one that is part of a native widget).  In this case, **Tcl_DoOneEvent** may not have a chance to service events so **Tcl_ServiceAll** must service them all.  Any recursive event loop that calls an external event loop rather than **Tcl_DoOneEvent** must reset the service mode so that all events get processed in **Tcl_ServiceAll**.  This is done by invoking the **Tcl_SetServiceMode** procedure.  If **Tcl_SetServiceMode** is passed **TCL_SERVICE_NONE**, then calls to **Tcl_ServiceAll** will return immediately without processing any events.  If **Tcl_SetServiceMode** is passed **TCL_SERVICE_ALL**, then calls to **Tcl_ServiceAll** will behave normally. **Tcl_SetServiceMode** returns the previous value of the service mode, which should be restored when the recursive loop exits. **Tcl_GetServiceMode** returns the current value of the service mode.

