/*
    Event system for RenderWare objects.
    Allows you to register event handlers to objects which can be triggered by the runtime.
*/

enum class event_t : uint32
{
    /*
        Every event that should be used in the framework is heavily recommended to be put down here.
        We do not want to have event ID number conflicts, and a unified event descriptor enumeration
        prevents that.
    */

    // Windowing system events.
    WINDOW_RESIZE,
    WINDOW_CLOSING,
    WINDOW_QUIT,
};

typedef void (__cdecl*EventHandler_t)( RwObject *obj, event_t triggeredEvent, void *callbackData, void *ud );

void RegisterEventHandler( RwObject *obj, event_t eventID, EventHandler_t theHandler, void *ud = NULL );
void UnregisterEventHandler( RwObject *obj, event_t eventID, EventHandler_t theHandler );

bool TriggerEvent( RwObject *obj, event_t eventID, void *ud );