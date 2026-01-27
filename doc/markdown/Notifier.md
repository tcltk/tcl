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

Tcl\_CreateEventSource, Tcl\_DeleteEventSource, Tcl\_SetMaxBlockTime, Tcl\_QueueEvent, Tcl\_ThreadQueueEvent, Tcl\_ThreadAlert, Tcl\_GetCurrentThread, Tcl\_DeleteEvents, Tcl\_InitNotifier, Tcl\_FinalizeNotifier, Tcl\_WaitForEvent, Tcl\_AlertNotifier, Tcl\_SetTimer, Tcl\_ServiceAll, Tcl\_ServiceEvent, Tcl\_GetServiceMode, Tcl\_SetServiceMode, Tcl\_ServiceModeHook, Tcl\_SetNotifier - the event queue and notifier interfaces

# Synopsis

::: {.synopsis} :::
**#include <tcl.h>**
[Tcl\_CreateEventSource]{.ccmd}[setupProc, checkProc, clientData]{.cargs}
[Tcl\_DeleteEventSource]{.ccmd}[setupProc, checkProc, clientData]{.cargs}
[Tcl\_SetMaxBlockTime]{.ccmd}[timePtr]{.cargs}
[Tcl\_QueueEvent]{.ccmd}[evPtr, position]{.cargs}
[Tcl\_ThreadQueueEvent]{.ccmd}[threadId, evPtr, position]{.cargs}
[Tcl\_ThreadAlert]{.ccmd}[threadId]{.cargs}
[Tcl\_ThreadId]{.ret} [Tcl\_GetCurrentThread]{.ccmd}[]{.cargs}
[Tcl\_DeleteEvents]{.ccmd}[deleteProc, clientData]{.cargs}
[void \*]{.ret} [Tcl\_InitNotifier]{.ccmd}[]{.cargs}
[Tcl\_FinalizeNotifier]{.ccmd}[clientData]{.cargs}
[int]{.ret} [Tcl\_WaitForEvent]{.ccmd}[timePtr]{.cargs}
[Tcl\_AlertNotifier]{.ccmd}[clientData]{.cargs}
[Tcl\_SetTimer]{.ccmd}[timePtr]{.cargs}
[int]{.ret} [Tcl\_ServiceAll]{.ccmd}[]{.cargs}
[int]{.ret} [Tcl\_ServiceEvent]{.ccmd}[flags]{.cargs}
[int]{.ret} [Tcl\_GetServiceMode]{.ccmd}[]{.cargs}
[int]{.ret} [Tcl\_SetServiceMode]{.ccmd}[mode]{.cargs}
[Tcl\_ServiceModeHook]{.ccmd}[mode]{.cargs}
[Tcl\_SetNotifier]{.ccmd}[notifierProcPtr]{.cargs}
:::

# Arguments

.AP Tcl\_EventSetupProc \*setupProc in Procedure to invoke to prepare for event wait in **Tcl\_DoOneEvent**. .AP Tcl\_EventCheckProc \*checkProc in Procedure for **Tcl\_DoOneEvent** to invoke after waiting for events.  Checks to see if any events have occurred and, if so, queues them. .AP void \*clientData in Arbitrary one-word value to pass to *setupProc*, *checkProc*, or *deleteProc*. .AP "const Tcl\_Time" \*timePtr in Indicates the maximum amount of time to wait for an event.  This is specified as an interval (how long to wait), not an absolute time (when to wakeup).  If the pointer passed to **Tcl\_WaitForEvent** is NULL, it means there is no maximum wait time:  wait forever if necessary. .AP Tcl\_Event \*evPtr in An event to add to the event queue.  The storage for the event must have been allocated by the caller using **Tcl\_Alloc**. .AP int position in Where to add the new event in the queue:  **TCL\_QUEUE\_TAIL**, **TCL\_QUEUE\_HEAD**, **TCL\_QUEUE\_MARK**, and whether to do an alert if the queue is empty: **TCL\_QUEUE\_ALERT\_IF\_EMPTY**. .AP Tcl\_ThreadId threadId in A unique identifier for a thread. .AP Tcl\_EventDeleteProc \*deleteProc in Procedure to invoke for each queued event in **Tcl\_DeleteEvents**. .AP int flags in What types of events to service.  These flags are the same as those passed to **Tcl\_DoOneEvent**. .AP int mode in Indicates whether events should be serviced by **Tcl\_ServiceAll**. Must be one of **TCL\_SERVICE\_NONE** or **TCL\_SERVICE\_ALL**. .AP const Tcl\_NotifierProcs\* notifierProcPtr in Structure of function pointers describing notifier procedures that are to replace the ones installed in the executable.  See [Replacing the notifier] for details.

# Introduction

The interfaces described here are used to customize the Tcl event loop.  The two most common customizations are to add new sources of events and to merge Tcl's event loop with some other event loop, such as one provided by an application in which Tcl is embedded.  Each of these tasks is described in a separate section below.

The procedures in this manual entry are the building blocks out of which the Tcl event notifier is constructed.  The event notifier is the lowest layer in the Tcl event mechanism.  It consists of three things:

1. Event sources: these represent the ways in which events can be generated.  For example, there is a timer event source that implements the **Tcl\_CreateTimerHandler** procedure and the [after] command, and there is a file event source that implements the **Tcl\_CreateFileHandler** procedure on Unix systems.  An event source must work with the notifier to detect events at the right times, record them on the event queue, and eventually notify higher-level software that they have occurred.  The procedures **Tcl\_CreateEventSource**, **Tcl\_DeleteEventSource**, and **Tcl\_SetMaxBlockTime**, **Tcl\_QueueEvent**, and **Tcl\_DeleteEvents** are used primarily by event sources.

2. The event queue: there is a single queue for each thread containing a Tcl interpreter, containing events that have been detected but not yet serviced.  Event sources place events onto the queue so that they may be processed in order at appropriate times during the event loop. The event queue guarantees a fair discipline of event handling, so that no event source can starve the others.  It also allows events to be saved for servicing at a future time. **Tcl\_QueueEvent** is used (primarily by event sources) to add events to the current thread's event queue and **Tcl\_DeleteEvents** is used to remove events from the queue without processing them.

3. The event loop: in order to detect and process events, the application enters a loop that waits for events to occur, places them on the event queue, and then processes them.  Most applications will do this by calling the procedure **Tcl\_DoOneEvent**, which is described in a separate manual entry.


Most Tcl applications need not worry about any of the internals of the Tcl notifier.  However, the notifier now has enough flexibility to be retargeted either for a new platform or to use an external event loop (such as the Motif event loop, when Tcl is embedded in a Motif application).  The procedures **Tcl\_WaitForEvent** and **Tcl\_SetTimer** are normally implemented by Tcl, but may be replaced with new versions to retarget the notifier (the **Tcl\_InitNotifier**, **Tcl\_AlertNotifier**, **Tcl\_FinalizeNotifier**, **Tcl\_Sleep**, **Tcl\_CreateFileHandler**, and **Tcl\_DeleteFileHandler** must also be replaced; see CREATING A NEW NOTIFIER below for details). The procedures **Tcl\_ServiceAll**, **Tcl\_ServiceEvent**, **Tcl\_GetServiceMode**, and **Tcl\_SetServiceMode** are provided to help connect Tcl's event loop to an external event loop such as Motif's.

# Notifier basics

The easiest way to understand how the notifier works is to consider what happens when **Tcl\_DoOneEvent** is called. **Tcl\_DoOneEvent** is passed a *flags* argument that indicates what sort of events it is OK to process and also whether or not to block if no events are ready.  **Tcl\_DoOneEvent** does the following things:

1. Check the event queue to see if it contains any events that can be serviced.  If so, service the first possible event, remove it from the queue, and return.  It does this by calling **Tcl\_ServiceEvent** and passing in the *flags* argument.

2. Prepare to block for an event.  To do this, **Tcl\_DoOneEvent** invokes a *setup procedure* in each event source. The event source will perform event-source specific initialization and possibly call **Tcl\_SetMaxBlockTime** to limit how long **Tcl\_WaitForEvent** will block if no new events occur.

3. Call **Tcl\_WaitForEvent**.  This procedure is implemented differently on different platforms;  it waits for an event to occur, based on the information provided by the event sources. It may cause the application to block if *timePtr* specifies an interval other than 0. **Tcl\_WaitForEvent** returns when something has happened, such as a file becoming readable or the interval given by *timePtr* expiring.  If there are no events for **Tcl\_WaitForEvent** to wait for, so that it would block forever, then it returns immediately and **Tcl\_DoOneEvent** returns 0.

4. Call a *check procedure* in each event source.  The check procedure determines whether any events of interest to this source occurred.  If so, the events are added to the event queue.

5. Check the event queue to see if it contains any events that can be serviced.  If so, service the first possible event, remove it from the queue, and return.

6. See if there are idle callbacks pending. If so, invoke all of them and return.

7. Either return 0 to indicate that no events were ready, or go back to step [2] if blocking was requested by the caller.


# Creating a new event source

An event source consists of three procedures invoked by the notifier, plus additional C procedures that are invoked by higher-level code to arrange for event-driven callbacks.  The three procedures called by the notifier consist of the setup and check procedures described above, plus an additional procedure that is invoked when an event is removed from the event queue for servicing.

The procedure **Tcl\_CreateEventSource** creates a new event source. Its arguments specify the setup procedure and check procedure for the event source. *SetupProc* should match the following prototype:

```
typedef void Tcl_EventSetupProc(
        void *clientData,
        int flags);
```

The *clientData* argument will be the same as the *clientData* argument to **Tcl\_CreateEventSource**;  it is typically used to point to private information managed by the event source. The *flags* argument will be the same as the *flags* argument passed to **Tcl\_DoOneEvent** except that it will never be 0 (**Tcl\_DoOneEvent** replaces 0 with **TCL\_ALL\_EVENTS**). *Flags* indicates what kinds of events should be considered; if the bit corresponding to this event source is not set, the event source should return immediately without doing anything.  For example, the file event source checks for the **TCL\_FILE\_EVENTS** bit.

*SetupProc*'s job is to make sure that the application wakes up when events of the desired type occur.  This is typically done in a platform-dependent fashion.  For example, under Unix an event source might call **Tcl\_CreateFileHandler**; under Windows it might request notification with a Windows event.  For timer-driven event sources such as timer events or any polled event, the event source can call **Tcl\_SetMaxBlockTime** to force the application to wake up after a specified time even if no events have occurred. If no event source calls **Tcl\_SetMaxBlockTime** then **Tcl\_WaitForEvent** will wait as long as necessary for an event to occur; otherwise, it will only wait as long as the shortest interval passed to **Tcl\_SetMaxBlockTime** by one of the event sources.  If an event source knows that it already has events ready to report, it can request a zero maximum block time.  For example, the setup procedure for the X event source looks to see if there are events already queued.  If there are, it calls **Tcl\_SetMaxBlockTime** with a 0 block time so that **Tcl\_WaitForEvent** does not block if there is no new data on the X connection. The *timePtr* argument to **Tcl\_WaitForEvent** points to a structure that describes a time interval in seconds and microseconds:

```
typedef struct {
    long long sec;
    long usec;
} Tcl_Time;
```

The *usec* field should be less than 1000000.

Information provided to **Tcl\_SetMaxBlockTime** is only used for the next call to **Tcl\_WaitForEvent**; it is discarded after **Tcl\_WaitForEvent** returns. The next time an event wait is done each of the event sources' setup procedures will be called again, and they can specify new information for that event wait.

If the application uses an external event loop rather than **Tcl\_DoOneEvent**, the event sources may need to call **Tcl\_SetMaxBlockTime** at other times.  For example, if a new event handler is registered that needs to poll for events, the event source may call **Tcl\_SetMaxBlockTime** to set the block time to zero to force the external event loop to call Tcl.  In this case, **Tcl\_SetMaxBlockTime** invokes **Tcl\_SetTimer** with the shortest interval seen since the last call to **Tcl\_DoOneEvent** or **Tcl\_ServiceAll**.

In addition to the generic procedure **Tcl\_SetMaxBlockTime**, other platform-specific procedures may also be available for *setupProc*, if there is additional information needed by **Tcl\_WaitForEvent** on that platform.  For example, on Unix systems the **Tcl\_CreateFileHandler** interface can be used to wait for file events.

The second procedure provided by each event source is its check procedure, indicated by the *checkProc* argument to **Tcl\_CreateEventSource**.  *CheckProc* must match the following prototype:

```
typedef void Tcl_EventCheckProc(
        void *clientData,
        int flags);
```

The arguments to this procedure are the same as those for *setupProc*. **CheckProc** is invoked by **Tcl\_DoOneEvent** after it has waited for events.  Presumably at least one event source is now prepared to queue an event.  **Tcl\_DoOneEvent** calls each of the event sources in turn, so they all have a chance to queue any events that are ready. The check procedure does two things.  First, it must see if any events have triggered.  Different event sources do this in different ways.

If an event source's check procedure detects an interesting event, it must add the event to Tcl's event queue.  To do this, the event source calls **Tcl\_QueueEvent**.  The *evPtr* argument is a pointer to a dynamically allocated structure containing the event (see below for more information on memory management issues).  Each event source can define its own event structure with whatever information is relevant to that event source.  However, the first element of the structure must be a structure of type **Tcl\_Event**, and the address of this structure is used when communicating between the event source and the rest of the notifier.  A **Tcl\_Event** has the following definition:

```
typedef struct Tcl_Event {
    Tcl_EventProc *proc;
    struct Tcl_Event *nextPtr;
} Tcl_Event;
```

The event source must fill in the *proc* field of the event before calling **Tcl\_QueueEvent**. The *nextPtr* is used to link together the events in the queue and should not be modified by the event source.

An event may be added to the queue at any of three positions, depending on the *position* argument to **Tcl\_QueueEvent**:

**TCL\_QUEUE\_TAIL**
: Add the event at the back of the queue, so that all other pending events will be serviced first.  This is almost always the right place for new events.

**TCL\_QUEUE\_HEAD**
: Add the event at the front of the queue, so that it will be serviced before all other queued events.

**TCL\_QUEUE\_MARK**
: Add the event at the front of the queue, unless there are other events at the front whose position is **TCL\_QUEUE\_MARK**;  if so, add the new event just after all other **TCL\_QUEUE\_MARK** events. This value of *position* is used to insert an ordered sequence of events at the front of the queue, such as a series of Enter and Leave events synthesized during a grab or ungrab operation in Tk.

**TCL\_QUEUE\_ALERT\_IF\_EMPTY**
: When used in **Tcl\_ThreadQueueEvent** arranges for an automatic call of **Tcl\_ThreadAlert** when the queue was empty.


When it is time to handle an event from the queue (steps 1 and 4 above) **Tcl\_ServiceEvent** will invoke the *proc* specified in the first queued **Tcl\_Event** structure. *Proc* must match the following prototype:

```
typedef int Tcl_EventProc(
        Tcl_Event *evPtr,
        int flags);
```

The first argument to *proc* is a pointer to the event, which will be the same as the first argument to the **Tcl\_QueueEvent** call that added the event to the queue. The second argument to *proc* is the *flags* argument for the current call to **Tcl\_ServiceEvent**;  this is used by the event source to return immediately if its events are not relevant.

It is up to *proc* to handle the event, typically by invoking one or more Tcl commands or C-level callbacks. Once the event source has finished handling the event it returns 1 to indicate that the event can be removed from the queue. If for some reason the event source decides that the event cannot be handled at this time, it may return 0 to indicate that the event should be deferred for processing later;  in this case **Tcl\_ServiceEvent** will go on to the next event in the queue and attempt to service it. There are several reasons why an event source might defer an event. One possibility is that events of this type are excluded by the *flags* argument. For example, the file event source will always return 0 if the **TCL\_FILE\_EVENTS** bit is not set in *flags*. Another example of deferring events happens in Tk if **Tk\_RestrictEvents** has been invoked to defer certain kinds of window events.

When *proc* returns 1, **Tcl\_ServiceEvent** will remove the event from the event queue and free its storage. Note that the storage for an event must be allocated by the event source (using **Tcl\_Alloc**) before calling **Tcl\_QueueEvent**, but it will be freed by **Tcl\_ServiceEvent**, not by the event source.

Calling **Tcl\_QueueEvent** adds an event to the current thread's queue. To add an event to another thread's queue, use **Tcl\_ThreadQueueEvent**. **Tcl\_ThreadQueueEvent** accepts as an argument a Tcl\_ThreadId argument, which uniquely identifies a thread in a Tcl application.  To obtain the Tcl\_ThreadId for the current thread, use the **Tcl\_GetCurrentThread** procedure.  (A thread would then need to pass this identifier to other threads for those threads to be able to add events to its queue.) After adding an event to another thread's queue, you then typically need to call **Tcl\_ThreadAlert** to "wake up" that thread's notifier to alert it to the new event.

**Tcl\_DeleteEvents** can be used to explicitly remove one or more events from the event queue.  **Tcl\_DeleteEvents** calls *proc* for each event in the queue, deleting those for with the procedure returns 1.  Events for which the procedure returns 0 are left in the queue.  *Proc* should match the following prototype:

```
typedef int Tcl_EventDeleteProc(
        Tcl_Event *evPtr,
        void *clientData);
```

The *clientData* argument will be the same as the *clientData* argument to **Tcl\_DeleteEvents**; it is typically used to point to private information managed by the event source.  The *evPtr* will point to the next event in the queue.

**Tcl\_DeleteEventSource** deletes an event source.  The *setupProc*, *checkProc*, and *clientData* arguments must exactly match those provided to the **Tcl\_CreateEventSource** for the event source to be deleted. If no such source exists, **Tcl\_DeleteEventSource** has no effect.

# Creating a new notifier

The notifier consists of all the procedures described in this manual entry, plus **Tcl\_DoOneEvent** and **Tcl\_Sleep**, which are available on all platforms, and **Tcl\_CreateFileHandler** and **Tcl\_DeleteFileHandler**, which are Unix-specific.  Most of these procedures are generic, in that they are the same for all notifiers. However, none of the procedures are notifier-dependent: **Tcl\_InitNotifier**, **Tcl\_AlertNotifier**, **Tcl\_FinalizeNotifier**, **Tcl\_SetTimer**, **Tcl\_Sleep**, **Tcl\_WaitForEvent**, **Tcl\_CreateFileHandler**, **Tcl\_DeleteFileHandler** and **Tcl\_ServiceModeHook**.  To support a new platform or to integrate Tcl with an application-specific event loop, you must write new versions of these procedures.

**Tcl\_InitNotifier** initializes the notifier state and returns a handle to the notifier state.  Tcl calls this procedure when initializing a Tcl interpreter.  Similarly, **Tcl\_FinalizeNotifier** shuts down the notifier, and is called by **Tcl\_Finalize** when shutting down a Tcl interpreter.

**Tcl\_WaitForEvent** is the lowest-level procedure in the notifier; it is responsible for waiting for an "interesting" event to occur or for a given time to elapse.  Before **Tcl\_WaitForEvent** is invoked, each of the event sources' setup procedure will have been invoked. The *timePtr* argument to **Tcl\_WaitForEvent** gives the maximum time to block for an event, based on calls to **Tcl\_SetMaxBlockTime** made by setup procedures and on other information (such as the **TCL\_DONT\_WAIT** bit in *flags*).

Ideally, **Tcl\_WaitForEvent** should only wait for an event to occur; it should not actually process the event in any way. Later on, the event sources will process the raw events and create Tcl\_Events on the event queue in their *checkProc* procedures. However, on some platforms (such as Windows) this is not possible; events may be processed in **Tcl\_WaitForEvent**, including queuing Tcl\_Events and more (for example, callbacks for native widgets may be invoked).  The return value from **Tcl\_WaitForEvent** must be either 0, 1, or -1.  On platforms such as Windows where events get processed in **Tcl\_WaitForEvent**, a return value of 1 means that there may be more events still pending that have not been processed.  This is a sign to the caller that it must call **Tcl\_WaitForEvent** again if it wants all pending events to be processed. A 0 return value means that calling **Tcl\_WaitForEvent** again will not have any effect: either this is a platform where **Tcl\_WaitForEvent** only waits without doing any event processing, or **Tcl\_WaitForEvent** knows for sure that there are no additional events to process (e.g. it returned because the time elapsed).  Finally, a return value of -1 means that the event loop is no longer operational and the application should probably unwind and terminate.  Under Windows this happens when a WM\_QUIT message is received; under Unix it happens when **Tcl\_WaitForEvent** would have waited forever because there were no active event sources and the timeout was infinite.

**Tcl\_AlertNotifier** is used to allow any thread to "wake up" the notifier to alert it to new events on its queue.  **Tcl\_AlertNotifier** requires as an argument the notifier handle returned by **Tcl\_InitNotifier**.

If the notifier will be used with an external event loop, then it must also support the **Tcl\_SetTimer** interface.  **Tcl\_SetTimer** is invoked by **Tcl\_SetMaxBlockTime** whenever the maximum blocking time has been reduced.  **Tcl\_SetTimer** should arrange for the external event loop to invoke **Tcl\_ServiceAll** after the specified interval even if no events have occurred.  This interface is needed because **Tcl\_WaitForEvent** is not invoked when there is an external event loop.  If the notifier will only be used from **Tcl\_DoOneEvent**, then **Tcl\_SetTimer** need not do anything.

**Tcl\_ServiceModeHook** is called by the platform-independent portion of the notifier when client code makes a call to **Tcl\_SetServiceMode**. This hook is provided to support operating systems that require special event handling when the application is in a modal loop (the Windows notifier, for instance, uses this hook to create a communication window).

On Unix systems, the file event source also needs support from the notifier.  The file event source consists of the **Tcl\_CreateFileHandler** and **Tcl\_DeleteFileHandler** procedures, which are described in the **Tcl\_CreateFileHandler** manual page.

The **Tcl\_Sleep** and **Tcl\_DoOneEvent** interfaces are described in their respective manual pages.

The easiest way to create a new notifier is to look at the code for an existing notifier, such as the files **unix/tclUnixNotfy.c** or **win/tclWinNotify.c** in the Tcl source distribution.

# Replacing the notifier

A notifier that has been written according to the conventions above can also be installed in a running process in place of the standard notifier.  This mechanism is used so that a single executable can be used (with the standard notifier) as a stand-alone program and reused (with a replacement notifier in a loadable extension) as an extension to another program, such as a Web browser plugin.

To do this, the extension makes a call to **Tcl\_SetNotifier** passing a pointer to a **Tcl\_NotifierProcs** data structure.  The structure has the following layout:

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

Following the call to **Tcl\_SetNotifier**, the pointers given in the **Tcl\_NotifierProcs** structure replace whatever notifier had been installed in the process.

It is extraordinarily unwise to replace a running notifier. Normally, **Tcl\_SetNotifier** should be called at process initialization time before the first call to **Tcl\_InitNotifier**.

# External event loops

The notifier interfaces are designed so that Tcl can be embedded into applications that have their own private event loops.  In this case, the application does not call **Tcl\_DoOneEvent** except in the case of recursive event loops such as calls to the Tcl commands [update] or [vwait].  Most of the time is spent in the external event loop of the application.  In this case the notifier must arrange for the external event loop to call back into Tcl when something happens on the various Tcl event sources.  These callbacks should arrange for appropriate Tcl events to be placed on the Tcl event queue.

Because the external event loop is not calling **Tcl\_DoOneEvent** on a regular basis, it is up to the notifier to arrange for **Tcl\_ServiceEvent** to be called whenever events are pending on the Tcl event queue.  The easiest way to do this is to invoke **Tcl\_ServiceAll** at the end of each callback from the external event loop.  This will ensure that all of the event sources are polled, any queued events are serviced, and any pending idle handlers are processed before returning control to the application.  In addition, event sources that need to poll for events can call **Tcl\_SetMaxBlockTime** to force the external event loop to call Tcl even if no events are available on the system event queue.

As a side effect of processing events detected in the main external event loop, Tcl may invoke **Tcl\_DoOneEvent** to start a recursive event loop in commands like [vwait].  **Tcl\_DoOneEvent** will invoke the external event loop, which will result in callbacks as described in the preceding paragraph, which will result in calls to **Tcl\_ServiceAll**.  However, in these cases it is undesirable to service events in **Tcl\_ServiceAll**.  Servicing events there is unnecessary because control will immediately return to the external event loop and hence to **Tcl\_DoOneEvent**, which can service the events itself.  Furthermore, **Tcl\_DoOneEvent** is supposed to service only a single event, whereas **Tcl\_ServiceAll** normally services all pending events.  To handle this situation, **Tcl\_DoOneEvent** sets a flag for **Tcl\_ServiceAll** that causes it to return without servicing any events. This flag is called the *service mode*; **Tcl\_DoOneEvent** restores it to its previous value before it returns.

In some cases, however, it may be necessary for **Tcl\_ServiceAll** to service events even when it has been invoked from **Tcl\_DoOneEvent**.  This happens when there is yet another recursive event loop invoked via an event handler called by **Tcl\_DoOneEvent** (such as one that is part of a native widget).  In this case, **Tcl\_DoOneEvent** may not have a chance to service events so **Tcl\_ServiceAll** must service them all.  Any recursive event loop that calls an external event loop rather than **Tcl\_DoOneEvent** must reset the service mode so that all events get processed in **Tcl\_ServiceAll**.  This is done by invoking the **Tcl\_SetServiceMode** procedure.  If **Tcl\_SetServiceMode** is passed **TCL\_SERVICE\_NONE**, then calls to **Tcl\_ServiceAll** will return immediately without processing any events.  If **Tcl\_SetServiceMode** is passed **TCL\_SERVICE\_ALL**, then calls to **Tcl\_ServiceAll** will behave normally. **Tcl\_SetServiceMode** returns the previous value of the service mode, which should be restored when the recursive loop exits. **Tcl\_GetServiceMode** returns the current value of the service mode.


[after]: after.md
[update]: update.md
[vwait]: vwait.md

