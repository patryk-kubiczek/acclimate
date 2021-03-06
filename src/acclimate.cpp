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

#include "acclimate.h"
#include <cfenv>
#include <csignal>
#include <ostream>
#include <string>
#include <utility>
#include "run.h"
#include "types.h"
#include "variants/ModelVariants.h"

namespace acclimate {

#ifdef FLOATING_POINT_EXCEPTIONS
static void handle_fpe_error(int sig) {
    UNUSED(sig);
    int exceptions = fetestexcept(FE_ALL_EXCEPT);
    feclearexcept(FE_ALL_EXCEPT);
    if (exceptions == 0) {
        return;
    }
    if (exceptions & FE_OVERFLOW) {
        warning_("FPE_OVERFLOW");
    }
    if (exceptions & FE_INVALID) {
        warning_("FPE_INVALID");
    }
    if (exceptions & FE_DIVBYZERO) {
        warning_("FPE_DIVBYZERO");
    }
    if (exceptions & FE_INEXACT) {
        warning_("FE_INEXACT");
    }
    if (exceptions & FE_UNDERFLOW) {
        warning_("FE_UNDERFLOW");
    }
#ifdef FATAL_FLOATING_POINT_EXCEPTIONS
    error_("Floating point exception");
#endif
}
#endif

Acclimate::Acclimate(settings::SettingsNode settings_p) {
#ifdef BANKERS_ROUNDING
    fesetround(FE_TONEAREST);
#endif

#ifdef FLOATING_POINT_EXCEPTIONS
    signal(SIGFPE, handle_fpe_error);
    feenableexcept(FE_OVERFLOW | FE_INVALID | FE_DIVBYZERO);
#endif
    const std::string& variant = settings_p["model"]["variant"].as<std::string>();
    if (variant == "basic") {
#ifdef VARIANT_BASIC
        variant_m = ModelVariantType::BASIC;
        run_m = std::make_shared<Run<VariantBasic>>(std::move(settings_p));
#else
        error_("Model variant '" << variant << "' not available in this binary");
#endif
    } else if (variant == "demand") {
#ifdef VARIANT_DEMAND
        variant_m = ModelVariantType::DEMAND;
        run_m = std::make_shared<Run<VariantDemand>>(std::move(settings_p));
#else
        error_("Model variant '" << variant << "' not available in this binary");
#endif
    } else if (variant == "prices") {
#ifdef VARIANT_PRICES
        variant_m = ModelVariantType::PRICES;
        run_m = std::make_shared<Run<VariantPrices>>(std::move(settings_p));
#else
        error_("Model variant '" << variant << "' not available in this binary");
#endif
    } else {
        error_("Unknown model variant '" << variant << "'");
    }
}

int Acclimate::run() {
    switch (variant_m) {
#ifdef VARIANT_BASIC
        case ModelVariantType::BASIC:
            return static_cast<Run<VariantBasic>*>(run_m.get())->run();
#endif
#ifdef VARIANT_DEMAND
        case ModelVariantType::DEMAND:
            return static_cast<Run<VariantDemand>*>(run_m.get())->run();
#endif
#ifdef VARIANT_PRICES
        case ModelVariantType::PRICES:
            return static_cast<Run<VariantPrices>*>(run_m.get())->run();
#endif
        default:
            return -1;
    }
}

}  // namespace acclimate
