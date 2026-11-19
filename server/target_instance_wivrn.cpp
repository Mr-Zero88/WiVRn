/*
 * WiVRn VR streaming
 * Copyright (C) 2025  Patrick Nicolas <patricknicolas@laposte.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "target_instance_wivrn.h"

#include "utils/method.h"
#include "utils/wivrn_trace.h"

#include "xrt/xrt_instance.h"
#include "xrt/xrt_system.h"

#include "b_system.h"
#include "b_app_policy.h"

#include <assert.h>

#include "driver/wivrn_session.h"
#include "wivrn_ipc.h"

namespace wivrn
{
xrt_result_t instance::is_system_available(bool * out_available)
{
	assert(out_available != NULL);
	*out_available = true;
	return XRT_SUCCESS;
}

xrt_result_t instance::create_system(
        struct xrt_system ** out_xsys,
        struct xrt_system_devices ** out_xsysd,
        struct xrt_space_overseer ** out_xspovrs,
        struct xrt_system_compositor ** out_xsysc)
{
	assert(out_xsysd != NULL);
	assert(*out_xsysd == NULL);
	assert(out_xsysc == NULL || *out_xsysc == NULL);
	assert(server);
	auto u_sys = b_system_create();
	*out_xsys = &u_sys->base;

	auto res = wivrn::wivrn_session::create_session(
	        std::move(connection),
	        *u_sys,
	        out_xsysd,
	        out_xspovrs,
	        out_xsysc);
	if (res != XRT_SUCCESS)
	{
		ipc_server_stop(server);
		return res;
	}
	session = (wivrn_session *)*out_xsysd;
	session->start(server);
	b_system_set_system_compositor(u_sys, *out_xsysc);
	return res;
}

xrt_result_t instance::get_prober(struct xrt_prober ** out_xp)
{
	*out_xp = nullptr;
	return XRT_ERROR_PROBER_NOT_SUPPORTED;
}

xrt_result_t instance::create_app_instance(struct xrt_app_instance **out_xainst)
{
	assert(out_xainst != NULL);
	assert(*out_xainst == NULL);

	struct b_app_instance *bainst = b_app_instance_create();
	if (bainst == NULL) {
		return XRT_ERROR_ALLOCATION;
	}

	*out_xainst = &bainst->base;

	// @note: We don't keep track of it internally, instead relying on external things to properly destroy their
	//        app instances before calling xrt_instance_destroy.

	return XRT_SUCCESS;
}

instance::instance() :
        xrt_instance{
                .is_system_available = method_pointer<&instance::is_system_available>,
                .create_system = method_pointer<&instance::create_system>,
                .get_prober = method_pointer<&instance::get_prober>,
				.create_app_instance = method_pointer<&instance::create_app_instance>,
                .destroy = [](xrt_instance * ptr) { wivrn::trace::shutdown(); delete (instance *)ptr; },
                .startup_timestamp = os_monotonic_get_ns(),
        }
{
}

void instance::set_ipc_server(ipc_server * server)
{
	this->server = server;
	if (session and not server)
		session->stop();
}

} // namespace wivrn
xrt_result_t
xrt_instance_create(struct xrt_instance_info * ii, struct xrt_instance ** out_xinst)
{
	wivrn::trace::init();

	*out_xinst = new wivrn::instance();

	return XRT_SUCCESS;
}
