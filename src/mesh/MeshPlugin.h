#pragma once

#include "mesh/Channels.h"
#include "mesh/MeshTypes.h"
#include <vector>

#ifndef NO_SCREEN
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>
#endif

/** handleReceived return enumeration
 * 
 * Use ProcessMessage::CONTINUE to allows other modules to process a message.
 * 
 * Use ProcessMessage::STOP to stop further message processing.
 */
enum class ProcessMessage
{
  CONTINUE = 0,
  STOP = 1,
};

/**
 * Used by modules to return the result of the AdminMessage handling.
 * If request is handled, then module should return HANDLED,
 * If response is also prepared for the request, then HANDLED_WITH_RESPONSE
 * should be returned.
 */
enum class AdminMessageHandleResult
{
    NOT_HANDLED = 0,
    HANDLED = 1,
    HANDLED_WITH_RESPONSE = 2,
};

/*
 * This struct is used by Screen to figure out whether screen frame should be updated.
 */
typedef struct _UIFrameEvent {
    bool frameChanged;
    bool needRedraw;
} UIFrameEvent;

/** A baseclass for any mesh "module".
 *
 * A module allows you to add new features to meshtastic device code, without needing to know messaging details.
 *
 * A key concept for this is that your module should use a particular "portnum" for each message type you want to receive
 * and handle.
 *
 * Interally we use modules to implement the core meshtastic text messaging and gps position sharing features.  You
 * can use these classes as examples for how to write your own custom module.  See here: (FIXME)
 */
class MeshPlugin
{
    static std::vector<MeshPlugin *> *modules;

  public:
    /** Constructor
     * name is for debugging output
     */
    MeshPlugin(const char *_name);

    virtual ~MeshPlugin();

    /** For use only by MeshService
     */
    static void callPlugins(const MeshPacket &mp, RxSource src = RX_SRC_RADIO);

    static std::vector<MeshPlugin *> GetMeshModulesWithUIFrames();
    static void observeUIEvents(Observer<const UIFrameEvent *> *observer);
    static AdminMessageHandleResult handleAdminMessageForAllPlugins(
        const MeshPacket &mp, AdminMessage *request, AdminMessage *response);
#ifndef NO_SCREEN
    virtual void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) { return; }
#endif
  protected:
    const char *name;

    /** Most modules only care about packets that are destined for their node (i.e. broadcasts or has their node as the specific
    recipient) But some plugs might want to 'sniff' packets that are merely being routed (passing through the current node). Those
    modules can set this to true and their handleReceived() will be called for every packet.
    */
    bool isPromiscuous = false;

    /** Also receive a copy of LOCALLY GENERATED messages - most modules should leave
     *  this setting disabled - see issue #877 */
    bool loopbackOk = false;

    /** Most modules only understand decrypted packets.  For modules that also want to see encrypted packets, they should set this
     * flag */
    bool encryptedOk = false;

    /** If a bound channel name is set, we will only accept received packets that come in on that channel.
     * A special exception (FIXME, not sure if this is a good idea) - packets that arrive on the local interface
     * are allowed on any channel (this lets the local user do anything).
     *
     * We will send responses on the same channel that the request arrived on.
     */
    const char *boundChannel = NULL;

    /**
     * If this module is currently handling a request currentRequest will be preset
     * to the packet with the request.  This is mostly useful for reply handlers.
     *
     * Note: this can be static because we are guaranteed to be processing only one
     * plumodulegin at a time.
     */
    static const MeshPacket *currentRequest;

    /**
     * If your handler wants to send a response, simply set currentReply and it will be sent at the end of response handling.
     */
    MeshPacket *myReply = NULL;

    /**
     * Initialize your module.  This setup function is called once after all hardware and mesh protocol layers have
     * been initialized
     */
    virtual void setup();

    /**
     * @return true if you want to receive the specified portnum
     */
    virtual bool wantPacket(const MeshPacket *p) = 0;

    /** Called to handle a particular incoming message

    @return ProcessMessage::STOP if you've guaranteed you've handled this message and no other handlers should be considered for it
    */
    virtual ProcessMessage handleReceived(const MeshPacket &mp) { return ProcessMessage::CONTINUE; }

    /** Messages can be received that have the want_response bit set.  If set, this callback will be invoked
     * so that subclasses can (optionally) send a response back to the original sender.
     *
     * Note: most implementers don't need to override this, instead: If while handling a request you have a reply, just set
     * the protected reply field in this instance.
     * */
    virtual MeshPacket *allocReply();

    /***
     * @return true if you want to be alloced a UI screen frame
     */
    virtual bool wantUIFrame() { return false; }
    virtual Observable<const UIFrameEvent *>* getUIFrameObservable() { return NULL; }

    MeshPacket *allocAckNak(Routing_Error err, NodeNum to, PacketId idFrom, ChannelIndex chIndex);

    /// Send an error response for the specified packet.
    MeshPacket *allocErrorResponse(Routing_Error err, const MeshPacket *p);

  /**
   * @brief An admin message arrived to AdminModule. Module was asked whether it want to handle the request.
   * 
   * @param mp The mesh packet arrived.
   * @param request The AdminMessage request extracted from the packet.
   * @param response The prepared response
   * @return AdminMessageHandleResult
   *   HANDLED if message was handled
   *   HANDLED_WITH_RESPONSE if a response is also prepared and to be sent.
   */
    virtual AdminMessageHandleResult handleAdminMessageForModule(
        const MeshPacket &mp, AdminMessage *request, AdminMessage *response) { return AdminMessageHandleResult::NOT_HANDLED; };

  private:
    /**
     * If any of the current chain of modules has already sent a reply, it will be here.  This is useful to allow
     * the RoutingModule to avoid sending redundant acks
     */
    static MeshPacket *currentReply;

    friend class ReliableRouter;

    /** Messages can be received that have the want_response bit set.  If set, this callback will be invoked
     * so that subclasses can (optionally) send a response back to the original sender.  This method calls allocReply()
     * to generate the reply message, and if !NULL that message will be delivered to whoever sent req
     */
    void sendResponse(const MeshPacket &req);
};

/** set the destination and packet parameters of packet p intended as a reply to a particular "to" packet
 * This ensures that if the request packet was sent reliably, the reply is sent that way as well.
 */
void setReplyTo(MeshPacket *p, const MeshPacket &to);
