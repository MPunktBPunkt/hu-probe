#pragma once

/**
 * @file ConnectedAppProbe.h
 * @brief Control-plane probe: C1a.5 lab BCL + C1b-0 skeleton + C1b-P0 AOA observe.
 *
 * No SAS/Etch/RHMI. BCL-on-wire is C1b-P1 (not auto-fed in P0).
 * Gadget mode isolation: link observe requires usb_gadget=aoa_observe boot.
 */

#include "connected/bcl/BclFramer.h"
#include "connected/transport/AoaTransport.h"
#include "msc/MscGadget.h"
#include "session/Session.h"

#include <ArduinoJson.h>

/**
 * @brief Device-side Connected-App instrumentation (lab + transport + P0 observe).
 */
class ConnectedAppProbe {
public:
    static ConnectedAppProbe& instance();

    /**
     * @brief Bind session, optional MSC, and gadget mode from config.
     * @param aoa_observe_boot  true when NVS usb_gadget=aoa_observe for this boot.
     */
    void begin(Session* session, MscGadget* msc = nullptr, bool aoa_observe_boot = false);

    /** @brief After AoaObserveGadget::begin — bind transport stack (P0). */
    void bindAoaObserveStack();

    /** @brief Poll transport / control tap while link observe is active. Arduino loop. */
    void loop();

    /**
     * @brief Scenario ca_c1_lab_bcl (C1a.5): synthetic stream → ca_bcl → stop.
     * @return true if 3 frames + 1 malformed published.
     */
    bool runCaC1LabBcl();

    /**
     * @brief Scenario ca_c1_link: arm AOA observe (P0) or skeleton (msc boot → fail).
     *
     * Requires aoa_observe boot for live stack. Passive: no BCL TX / no auto-BclFramer.
     */
    bool startCaC1Link();

    /** @brief Stop ca_c1_link observe session and disarm AOA. */
    bool stopCaC1Link();

    /**
     * @brief Lab-only: inject hex bytes into AoaTransport RX (not NBT wire).
     * In P0 observe, inject still goes to ring + optional lab BclFramer path only if
     * feed_framer_lab_ is set; default P0 publishes ca_usb bulk only.
     */
    bool injectRxHex(const char* hex);

    /** @brief Lab-only: mark AOA skeleton connected/disconnected without USB. */
    bool labSetConnected(bool up);

    void toStatusJson(JsonObject obj) const;

    const char* lastErr() const { return lastErr_; }
    bool linkObserving() const { return link_observing_; }
    bool aoaObserveBoot() const { return aoa_observe_boot_; }

private:
    ConnectedAppProbe() = default;

    void publishFrame(const huprobe::bcl::BclFrame& f, bool from_transport);
    void publishError(const huprobe::bcl::BclFeedResult& r, bool from_transport);
    void publishTransportEvent(huprobe::transport::TransportEvent ev, uint32_t n = 0,
                               const char* note = nullptr);
    void publishCaUsb(const char* p0_level, const char* ca_ev, bool hypothesis,
                      bool direct_obs, JsonDocument& extra);
    void drainControlTap();
    void drainTransportRaw();
    void resetCounters();
    void drainTransportToFramer();  // lab inject / C1b-0 skeleton only

    Session* session_ = nullptr;
    MscGadget* msc_ = nullptr;
    huprobe::bcl::BclFramer framer_;
    huprobe::transport::AoaTransport aoa_;

    bool aoa_observe_boot_ = false;
    bool running_ = false;
    bool link_observing_ = false;
    bool feed_framer_on_inject_ = true;  // lab inject still exercises framer (TEST)
    const char* state_ = "idle";
    const char* lastErr_ = "";
    const char* scenario_ = "ca_c1_lab_bcl";
    uint32_t frames_ = 0;
    uint32_t malformed_ = 0;
    uint32_t incomplete_obs_ = 0;
    uint32_t unknown_cmd_ = 0;
    uint32_t runs_ = 0;
    uint32_t link_runs_ = 0;
    uint32_t ca_usb_count_ = 0;
    bool p0_a_ = false;
    bool p0_b_ = false;
    bool p0_c_ = false;
    bool p0_d_ = false;
    bool p0_e_ = false;
    uint16_t last_cmd_ = 0;
    uint16_t last_src_ = 0;
    uint16_t last_dest_ = 0;
    uint16_t last_len_ = 0;
    bool has_last_ = false;
    String last_sid_;
    bool msc_detached_for_link_ = false;
    uint32_t last_attach_seen_ = 0;
};
