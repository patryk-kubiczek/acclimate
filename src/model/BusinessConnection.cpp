/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
                          Christian Otto <christian.otto@pik-potsdam.de>

  This file is part of Acclimate.

  Acclimate is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Acclimate.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "model/BusinessConnection.h"

#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/SalesManager.h"
#include "model/Storage.h"
#include "model/TransportChainLink.h"
#include "run.h"

namespace acclimate {

BusinessConnection::BusinessConnection(PurchasingManager* buyer_p, SalesManager* seller_p, const Flow& initial_flow_Z_star_p)
    : buyer(buyer_p),
      demand_fulfill_history_(1.0),
      initial_flow_Z_star_(initial_flow_Z_star_p),
      last_delivery_Z_(initial_flow_Z_star_p),
      last_demand_request_D_(initial_flow_Z_star_p),
      seller(seller_p),
      transport_costs(0.0),
      last_shipment_Z_(initial_flow_Z_star_p),
      time_(seller_p->model()->time()) {
    if (seller->firm->sector->transport_type == Sector::TransportType::IMMEDIATE || buyer->storage->economic_agent->region == seller->firm->region) {
        first_transport_link.reset(new TransportChainLink(this, 0, initial_flow_Z_star_p, nullptr));
    } else {
        const auto& route = seller->firm->region->find_path_to(buyer->storage->economic_agent->region, seller->firm->sector->transport_type);
        assert(route.path.size() > 0);
        TransportChainLink* link;
        for (std::size_t i = 0; i < route.path.size(); ++i) {
            GeoEntity* p = route.path[i];
            auto new_link = new TransportChainLink(this, p->delay, initial_flow_Z_star_p, p);
            if (i == 0) {
                first_transport_link.reset(new_link);
            } else {
                link->next_transport_chain_link.reset(new_link);
            }
            link = new_link;
        }
    }
}

FloatType BusinessConnection::get_minimum_passage() const {
    TransportChainLink* link = first_transport_link.get();
    FloatType minimum_passage = 1.0;
    FloatType link_passage;
    while (link) {
        link_passage = link->get_passage();
        if (link_passage >= 0.0 && link_passage < minimum_passage) {
            minimum_passage = link_passage;
        }
        link = link->next_transport_chain_link.get();
    }
    if (minimum_passage > 1.0 || minimum_passage < 0.0) {
        minimum_passage = 1.0;
    }
    return minimum_passage;
}

bool BusinessConnection::get_domestic() const { return (buyer->storage->economic_agent->region == seller->firm->region); }

void BusinessConnection::push_flow_Z(const Flow& flow_Z) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    last_shipment_Z_ = round(flow_Z);
    first_transport_link->push_flow_Z(last_shipment_Z_, initial_flow_Z_star_.get_quantity());
    if (!get_domestic()) {
        seller->firm->region->add_export_Z(last_shipment_Z_);
    }
}

TransportDelay BusinessConnection::get_transport_delay_tau() const {
    TransportChainLink* link = first_transport_link.get();
    TransportDelay res = 0;
    while (link) {
        res += link->transport_delay();
        link = link->next_transport_chain_link.get();
    }
    return res;
}

void BusinessConnection::deliver_flow_Z(const Flow& flow_Z) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    buyer->storage->push_flow_Z(flow_Z);
    last_delivery_Z_ = flow_Z;
    if (!get_domestic()) {
        buyer->storage->economic_agent->region->add_import_Z(flow_Z);
    }
}

std::size_t BusinessConnection::get_id(const TransportChainLink* transport_chain_link) const {
    std::size_t id = 0;
    if (first_transport_link) {
        const TransportChainLink* link = first_transport_link.get();
        while (link->next_transport_chain_link && transport_chain_link != link->next_transport_chain_link.get()) {
            link = link->next_transport_chain_link.get();
            id++;
        }
    }
    return id;
}

void BusinessConnection::send_demand_request_D(const Demand& demand_request_D) {
    assertstep(PURCHASE);
    last_demand_request_D_ = round(demand_request_D);
    seller->add_demand_request_D(last_demand_request_D_);
}

Flow BusinessConnection::get_flow_mean() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link.get();
    Flow res = last_delivery_Z_;
    TransportDelay delay = 0;
    while (link) {
        res += link->get_total_flow();
        delay += link->transport_delay();
        link = link->next_transport_chain_link.get();
    }
    return round(res / Ratio(delay));
}

FlowQuantity BusinessConnection::get_flow_deficit() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link.get();
    FlowQuantity res = initial_flow_Z_star_.get_quantity() - last_delivery_Z_.get_quantity();
    while (link) {
        res += link->get_flow_deficit();
        link = link->next_transport_chain_link.get();
    }
    return round(res);
}

Flow BusinessConnection::get_total_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    return round(get_transport_flow() + last_delivery_Z_);
}

Flow BusinessConnection::get_transport_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link.get();
    Flow res = Flow(0.0);
    while (link) {
        res += link->get_total_flow();
        link = link->next_transport_chain_link.get();
    }
    return round(res);
}

Flow BusinessConnection::get_disequilibrium() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link.get();
    Flow res = Flow(0.0);
    while (link) {
        res.add_possibly_negative(link->get_disequilibrium());
        link = link->next_transport_chain_link.get();
    }
    return round(res, true);
}

FloatType BusinessConnection::get_stddeviation() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link.get();
    FloatType res = 0.0;
    while (link) {
        res += link->get_stddeviation();
        link = link->next_transport_chain_link.get();
    }
    return res;
}

Model* BusinessConnection::model() const { return buyer->model(); }

std::string BusinessConnection::id() const { return (seller ? seller->id() : "INVALID") + "->" + (buyer ? buyer->storage->economic_agent->id() : "INVALID"); }

const Flow& BusinessConnection::last_shipment_Z(const SalesManager* const caller) const {
    if constexpr (DEBUG_MODE) {
        if (caller != seller) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
        }
    } else {
        UNUSED(caller);
    }
    return last_shipment_Z_;
}

const Flow& BusinessConnection::last_delivery_Z(const SalesManager* const caller) const {
    if constexpr (DEBUG_MODE) {
        if (caller != seller) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
        }
    } else {
        UNUSED(caller);
    }
    return last_delivery_Z_;
}

const Demand& BusinessConnection::last_demand_request_D(const PurchasingManager* const caller) const {
    if constexpr (DEBUG_MODE) {
        if (caller != buyer) {
            assertstepnot(PURCHASE);
        }
    } else {
        UNUSED(caller);
    }
    return last_demand_request_D_;
}

void BusinessConnection::initial_flow_Z_star(const Flow& new_initial_flow_Z_star) {
    assertstep(INVESTMENT);
    initial_flow_Z_star_ = new_initial_flow_Z_star;
}

}  // namespace acclimate
