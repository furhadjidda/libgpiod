// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2017-2021 Bartosz Golaszewski <bartekgola@gmail.com>

#include <glib.h>
#include <gpiod.h>
#include <poll.h>

#include "gpiod-test.h"
#include "gpiod-test-helpers.h"
#include "gpiod-test-sim.h"

#define GPIOD_TEST_GROUP "info-event"

GPIOD_TEST_CASE(watching_info_events_returns_line_info)
{
	g_autoptr(GPIOSimChip) sim = g_gpiosim_chip_new("num-lines", 8, NULL);
	g_autoptr(struct_gpiod_chip) chip = NULL;
	g_autoptr(struct_gpiod_line_info) info = NULL;

	chip = gpiod_test_open_chip_or_fail(g_gpiosim_chip_get_dev_path(sim));

	info = gpiod_chip_watch_line_info(chip, 3);
	g_assert_nonnull(info);
	g_assert_cmpuint(gpiod_line_info_get_offset(info), ==, 3);
}

GPIOD_TEST_CASE(try_offset_out_of_range)
{
	g_autoptr(GPIOSimChip) sim = g_gpiosim_chip_new("num-lines", 8, NULL);
	g_autoptr(struct_gpiod_chip) chip = NULL;
	g_autoptr(struct_gpiod_line_info) info = NULL;

	chip = gpiod_test_open_chip_or_fail(g_gpiosim_chip_get_dev_path(sim));

	info = gpiod_chip_watch_line_info(chip, 10);
	g_assert_null(info);
	gpiod_test_expect_errno(EINVAL);
}

GPIOD_TEST_CASE(event_timeout)
{
	g_autoptr(GPIOSimChip) sim = g_gpiosim_chip_new("num-lines", 8, NULL);
	g_autoptr(struct_gpiod_chip) chip = NULL;
	g_autoptr(struct_gpiod_line_info) info = NULL;
	gint ret;

	chip = gpiod_test_open_chip_or_fail(g_gpiosim_chip_get_dev_path(sim));

	info = gpiod_chip_watch_line_info(chip, 6);
	g_assert_nonnull(info);
	gpiod_test_return_if_failed();

	ret = gpiod_chip_info_event_wait(chip, 100000000);
	g_assert_cmpint(ret, ==, 0);
}

struct request_ctx {
	struct gpiod_chip *chip;
	struct gpiod_request_config *req_cfg;
	struct gpiod_line_config *line_cfg;
};

static gpointer request_reconfigure_release_line(gpointer data)
{
	g_autoptr(struct_gpiod_line_request) request = NULL;
	struct request_ctx *ctx = data;
	gint ret;

	g_usleep(1000);

	request = gpiod_chip_request_lines(ctx->chip,
					   ctx->req_cfg, ctx->line_cfg);
	g_assert_nonnull(request);
	if (g_test_failed())
		return NULL;

	g_usleep(1000);

	gpiod_line_config_set_direction_default(ctx->line_cfg,
						GPIOD_LINE_DIRECTION_OUTPUT);

	ret = gpiod_line_request_reconfigure_lines(request, ctx->line_cfg);
	g_assert_cmpint(ret, ==, 0);
	if (g_test_failed())
		return NULL;

	g_usleep(1000);

	gpiod_line_request_release(request);
	request = NULL;

	return NULL;
}

GPIOD_TEST_CASE(request_reconfigure_release_events)
{
	static const guint offset = 3;

	g_autoptr(GPIOSimChip) sim = g_gpiosim_chip_new("num-lines", 8, NULL);
	g_autoptr(struct_gpiod_chip) chip = NULL;
	g_autoptr(struct_gpiod_line_info) info = NULL;
	g_autoptr(struct_gpiod_info_event) request_event = NULL;
	g_autoptr(struct_gpiod_info_event) reconfigure_event = NULL;
	g_autoptr(struct_gpiod_info_event) release_event = NULL;
	g_autoptr(struct_gpiod_request_config) req_cfg = NULL;
	g_autoptr(struct_gpiod_line_config) line_cfg = NULL;
	g_autoptr(GThread) thread = NULL;
	struct gpiod_line_info *request_info, *reconfigure_info, *release_info;
	guint64 request_ts, reconfigure_ts, release_ts;
	struct request_ctx ctx;
	gint ret;

	chip = gpiod_test_open_chip_or_fail(g_gpiosim_chip_get_dev_path(sim));
	req_cfg = gpiod_test_create_request_config_or_fail();
	line_cfg = gpiod_test_create_line_config_or_fail();

	gpiod_request_config_set_offsets(req_cfg, 1, &offset);

	info = gpiod_chip_watch_line_info(chip, 3);
	g_assert_nonnull(info);
	gpiod_test_return_if_failed();

	g_assert_false(gpiod_line_info_is_used(info));

	ctx.chip = chip;
	ctx.req_cfg = req_cfg;
	ctx.line_cfg = line_cfg;

	thread = g_thread_new("request-release",
			      request_reconfigure_release_line, &ctx);
	g_thread_ref(thread);

	ret = gpiod_chip_info_event_wait(chip, 1000000000);
	g_assert_cmpint(ret, >, 0);
	gpiod_test_join_thread_and_return_if_failed(thread);

	request_event = gpiod_chip_info_event_read(chip);
	g_assert_nonnull(request_event);
	gpiod_test_join_thread_and_return_if_failed(thread);

	g_assert_cmpint(gpiod_info_event_get_event_type(request_event), ==,
			GPIOD_INFO_EVENT_LINE_REQUESTED);

	request_info = gpiod_info_event_get_line_info(request_event);

	g_assert_cmpuint(gpiod_line_info_get_offset(request_info), ==, 3);
	g_assert_true(gpiod_line_info_is_used(request_info));
	g_assert_cmpint(gpiod_line_info_get_direction(request_info), ==,
			GPIOD_LINE_DIRECTION_INPUT);

	ret = gpiod_chip_info_event_wait(chip, 1000000000);
	g_assert_cmpint(ret, >, 0);
	gpiod_test_join_thread_and_return_if_failed(thread);

	reconfigure_event = gpiod_chip_info_event_read(chip);
	g_assert_nonnull(reconfigure_event);
	gpiod_test_join_thread_and_return_if_failed(thread);

	g_assert_cmpint(gpiod_info_event_get_event_type(reconfigure_event), ==,
			GPIOD_INFO_EVENT_LINE_CONFIG_CHANGED);

	reconfigure_info = gpiod_info_event_get_line_info(reconfigure_event);

	g_assert_cmpuint(gpiod_line_info_get_offset(reconfigure_info), ==, 3);
	g_assert_true(gpiod_line_info_is_used(reconfigure_info));
	g_assert_cmpint(gpiod_line_info_get_direction(reconfigure_info), ==,
			GPIOD_LINE_DIRECTION_OUTPUT);

	ret = gpiod_chip_info_event_wait(chip, 1000000000);
	g_assert_cmpint(ret, >, 0);
	gpiod_test_join_thread_and_return_if_failed(thread);

	release_event = gpiod_chip_info_event_read(chip);
	g_assert_nonnull(release_event);
	gpiod_test_join_thread_and_return_if_failed(thread);

	g_assert_cmpint(gpiod_info_event_get_event_type(release_event), ==,
			GPIOD_INFO_EVENT_LINE_RELEASED);

	release_info = gpiod_info_event_get_line_info(release_event);

	g_assert_cmpuint(gpiod_line_info_get_offset(release_info), ==, 3);
	g_assert_false(gpiod_line_info_is_used(release_info));

	g_thread_join(thread);

	request_ts = gpiod_info_event_get_timestamp(request_event);
	reconfigure_ts = gpiod_info_event_get_timestamp(reconfigure_event);
	release_ts = gpiod_info_event_get_timestamp(release_event);

	g_assert_cmpuint(request_ts, <, reconfigure_ts);
	g_assert_cmpuint(reconfigure_ts, <, release_ts);
}

GPIOD_TEST_CASE(chip_fd_can_be_polled)
{
	static const guint offset = 3;

	g_autoptr(GPIOSimChip) sim = g_gpiosim_chip_new("num-lines", 8, NULL);
	g_autoptr(struct_gpiod_chip) chip = NULL;
	g_autoptr(struct_gpiod_line_info) info = NULL;
	g_autoptr(struct_gpiod_info_event) event = NULL;
	g_autoptr(struct_gpiod_request_config) req_cfg = NULL;
	g_autoptr(struct_gpiod_line_config) line_cfg = NULL;
	g_autoptr(struct_gpiod_line_request) request = NULL;
	g_autoptr(GThread) thread = NULL;
	struct gpiod_line_info *evinfo;
	struct request_ctx ctx;
	struct timespec ts;
	struct pollfd pfd;
	gint ret, fd;

	chip = gpiod_test_open_chip_or_fail(g_gpiosim_chip_get_dev_path(sim));
	req_cfg = gpiod_test_create_request_config_or_fail();
	line_cfg = gpiod_test_create_line_config_or_fail();

	gpiod_request_config_set_offsets(req_cfg, 1, &offset);

	info = gpiod_chip_watch_line_info(chip, 3);
	g_assert_nonnull(info);
	gpiod_test_return_if_failed();

	g_assert_false(gpiod_line_info_is_used(info));

	ctx.chip = chip;
	ctx.req_cfg = req_cfg;
	ctx.line_cfg = line_cfg;

	thread = g_thread_new("request-release",
			      request_reconfigure_release_line, &ctx);
	g_thread_ref(thread);

	fd = gpiod_chip_get_fd(chip);

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN | POLLPRI;

	ts.tv_sec = 1;
	ts.tv_nsec = 0;

	ret = ppoll(&pfd, 1, &ts, NULL);
	g_assert_cmpint(ret, >, 0);
	gpiod_test_join_thread_and_return_if_failed(thread);

	event = gpiod_chip_info_event_read(chip);
	g_assert_nonnull(event);
	gpiod_test_join_thread_and_return_if_failed(thread);

	g_assert_cmpint(gpiod_info_event_get_event_type(event), ==,
			GPIOD_INFO_EVENT_LINE_REQUESTED);

	evinfo = gpiod_info_event_get_line_info(event);

	g_assert_cmpuint(gpiod_line_info_get_offset(evinfo), ==, 3);
	g_assert_true(gpiod_line_info_is_used(evinfo));

	g_thread_join(thread);
}

GPIOD_TEST_CASE(unwatch_and_check_that_no_events_are_generated)
{
	static const guint offset = 3;

	g_autoptr(GPIOSimChip) sim = g_gpiosim_chip_new("num-lines", 8, NULL);
	g_autoptr(struct_gpiod_chip) chip = NULL;
	g_autoptr(struct_gpiod_line_info) info = NULL;
	g_autoptr(struct_gpiod_info_event) event = NULL;
	g_autoptr(struct_gpiod_request_config) req_cfg = NULL;
	g_autoptr(struct_gpiod_line_config) line_cfg = NULL;
	g_autoptr(struct_gpiod_line_request) request = NULL;
	gint ret;

	chip = gpiod_test_open_chip_or_fail(g_gpiosim_chip_get_dev_path(sim));
	req_cfg = gpiod_test_create_request_config_or_fail();
	line_cfg = gpiod_test_create_line_config_or_fail();

	gpiod_request_config_set_offsets(req_cfg, 1, &offset);

	info = gpiod_chip_watch_line_info(chip, 3);
	g_assert_nonnull(info);
	gpiod_test_return_if_failed();

	request = gpiod_test_request_lines_or_fail(chip, req_cfg, line_cfg);

	ret = gpiod_chip_info_event_wait(chip, 100000000);
	g_assert_cmpint(ret, >, 0);
	gpiod_test_return_if_failed();

	event = gpiod_chip_info_event_read(chip);
	g_assert_nonnull(event);
	gpiod_test_return_if_failed();

	ret = gpiod_chip_unwatch_line_info(chip, 3);
	g_assert_cmpint(ret, ==, 0);
	gpiod_test_return_if_failed();

	gpiod_line_request_release(request);
	request = NULL;

	ret = gpiod_chip_info_event_wait(chip, 100000000);
	g_assert_cmpint(ret, ==, 0);
}
