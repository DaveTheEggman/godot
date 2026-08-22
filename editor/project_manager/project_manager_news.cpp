/**************************************************************************/
/*  project_manager_news.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "project_manager_news.h"

#include "core/io/http_client.h"
#include "core/io/xml_parser.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/string/translation.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/theme/theme_db.h"
#include "servers/display/display_server.h"

ProjectManagerNews::ProjectManagerNews() {
	set_name("ProjectManagerNews");
	set_v_size_flags(Control::SIZE_EXPAND_FILL);

	// Notification banner.
	{
		notification_banner = memnew(PanelContainer);
		add_child(notification_banner);

		MarginContainer *banner_margin = memnew(MarginContainer);
		banner_margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
		banner_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
		banner_margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
		banner_margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
		notification_banner->add_child(banner_margin);

		HBoxContainer *banner_hbox = memnew(HBoxContainer);
		banner_hbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		banner_margin->add_child(banner_hbox);

		notification_label = memnew(Label);
		notification_label->set_text(TTR("Activate the News tab to get Godot community updates and news."));
		notification_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		banner_hbox->add_child(notification_label);

		notification_activate_button = memnew(Button);
		notification_activate_button->set_text(TTR("Activate News"));
		banner_hbox->add_child(notification_activate_button);

		notification_dismiss_button = memnew(Button);
		notification_dismiss_button->set_text(TTR("Don't Show Again"));
		banner_hbox->add_child(notification_dismiss_button);

		notification_activate_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectManagerNews::_notification_banner_activate));

		notification_dismiss_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectManagerNews::_notification_banner_dismiss));
	}

	// News content.
	{
		news_scroll = memnew(ScrollContainer);
		news_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		news_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
		add_child(news_scroll);

		news_list = memnew(VBoxContainer);
		news_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		news_list->add_theme_constant_override("separation", 12 * EDSCALE);
		news_scroll->add_child(news_list);
	}

	// Status/refresh controls.
	{
		HBoxContainer *controls = memnew(HBoxContainer);
		add_child(controls);

		status_label = memnew(Label);
		status_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		controls->add_child(status_label);

		refresh_button = memnew(Button);
		refresh_button->set_text(TTR("Refresh"));
		controls->add_child(refresh_button);

		refresh_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectManagerNews::_refresh_pressed));
	}

	_set_news_state(NEWS_STATE_IDLE);
}

ProjectManagerNews::~ProjectManagerNews() {
	if (http_client) {
		memdelete(http_client);
		http_client = nullptr;
	}
}

void ProjectManagerNews::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_update_theme();
		} break;

		case NOTIFICATION_PROCESS: {
			if (http_client) {
				_poll_http();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
		} break;
	}
}

void ProjectManagerNews::_update_theme() {
	if (!is_inside_tree()) {
		return;
	}

	if (refresh_button) {
		refresh_button->set_button_icon(get_theme_icon("Reload", "EditorIcons"));
	}

	if (notification_banner) {
		notification_banner->add_theme_style_override(SceneStringName(panel), get_theme_stylebox("panel", "ProjectManager"));
	}
}

void ProjectManagerNews::_set_news_state(NewsState p_state) {
	news_state = p_state;

	switch (news_state) {
		case NEWS_STATE_IDLE: {
			status_label->set_text(TTR("News requires online access."));
			refresh_button->set_disabled(false);
		} break;

		case NEWS_STATE_CONNECTING:
		case NEWS_STATE_LOADING: {
			status_label->set_text(TTR("Loading news..."));
			refresh_button->set_disabled(true);
		} break;

		case NEWS_STATE_ERROR: {
			refresh_button->set_disabled(false);
		} break;

		case NEWS_STATE_READY: {
			status_label->set_text(vformat(TTR("%d news items"), news_items.size()));
			refresh_button->set_disabled(false);
		} break;
	}
}

void ProjectManagerNews::activate() {
	notification_banner->hide();
	refresh();
}

void ProjectManagerNews::refresh() {
	const int network_mode = EDITOR_GET("network/connection/network_mode");

	if (network_mode == EditorSettings::NETWORK_OFFLINE) {
		_set_news_state(NEWS_STATE_IDLE);
		return;
	}

	_fetch_news();
}

bool ProjectManagerNews::is_loading() const {
	return news_state == NEWS_STATE_CONNECTING || news_state == NEWS_STATE_LOADING;
}

void ProjectManagerNews::_fetch_news() {
	if (http_client) {
		return;
	}

	_clear_news();

	http_client = HTTPClient::create();
	if (!http_client) {
		_show_error(TTR("Unable to create an HTTP client."));
		return;
	}

	http_client->set_blocking_mode(false);

	_set_news_state(NEWS_STATE_CONNECTING);

	const String url = NEWS_RSS_URL;

	String scheme = url.get_slice("://", 0);
	String host = url.get_slice("://", 1).get_slice("/", 0);

	if (scheme != "https") {
		_show_error(TTR("The news feed must use HTTPS."));
		memdelete(http_client);
		http_client = nullptr;
		return;
	}

	Error err = http_client->connect_to_host(host, 443, TLSOptions::client());
	if (err != OK) {
		_show_error(TTR("Unable to connect to the news server."));
		memdelete(http_client);
		http_client = nullptr;
		return;
	}

	set_process(true);
}

void ProjectManagerNews::_poll_http() {
	ERR_FAIL_NULL(http_client);

	http_client->poll();

	switch (http_client->get_status()) {
		case HTTPClient::STATUS_CONNECTED: {
			Vector<String> headers;
			headers.push_back("User-Agent: Godot/" + GODOT_VERSION_NAME);
			headers.push_back("Accept: application/rss+xml, application/xml, text/xml");

			Error err = http_client->request(HTTPClient::METHOD_GET, "/rss.xml", headers);

			if (err != OK) {
				_show_error(TTR("Unable to request the news feed."));
				return;
			}

			_set_news_state(NEWS_STATE_LOADING);
		} break;

		case HTTPClient::STATUS_BODY: {
			PackedByteArray chunk = http_client->read_response_body_chunk();

			if (!chunk.is_empty()) {
				response_body.append_array(chunk);
			}

			if (!http_client->has_response()) {
				return;
			}
		} break;

		case HTTPClient::STATUS_CONNECTION_ERROR:
		case HTTPClient::STATUS_CANT_CONNECT:
		case HTTPClient::STATUS_CANT_RESOLVE:
		case HTTPClient::STATUS_TLS_HANDSHAKE_ERROR: {
			_show_error(TTR("Unable to connect to the Godot news feed."));
			return;
		} break;

		default:
			break;
	}

	if (http_client->get_status() == HTTPClient::STATUS_DISCONNECTED ||
			http_client->get_status() == HTTPClient::STATUS_CONNECTION_ERROR) {
		return;
	}

	if (http_client->has_response()) {
		const int response_code = http_client->get_response_code();

		if (response_code < 200 || response_code >= 300) {
			_show_error(vformat(TTR("The news server returned HTTP status %d."), response_code));
			return;
		}

		if (http_client->get_status() != HTTPClient::STATUS_BODY) {
			_parse_news(response_body);

			memdelete(http_client);
			http_client = nullptr;
			set_process(false);
		}
	}
}

void ProjectManagerNews::_parse_news(const PackedByteArray &p_data) {
	_clear_news();

	Ref<XMLParser> parser;
	parser.instantiate();

	Error err = parser->open_buffer(p_data);
	if (err != OK) {
		_show_error(TTR("Unable to parse the news feed."));
		return;
	}

	bool inside_item = false;
	NewsItem current_item;
	String current_element;

	while (parser->read() == OK) {
		switch (parser->get_node_type()) {
			case XMLParser::NODE_ELEMENT: {
				current_element = parser->get_node_name();

				if (current_element == "item") {
					inside_item = true;
					current_item = NewsItem();
				}
			} break;

			case XMLParser::NODE_TEXT: {
				if (!inside_item) {
					break;
				}

				const String text = parser->get_node_data();

				if (current_element == "title") {
					current_item.title = text;
				} else if (current_element == "link") {
					current_item.link = text;
				} else if (current_element == "description") {
					current_item.description = text;
				} else if (current_element == "pubDate") {
					current_item.pub_date = text;
				}
			} break;

			case XMLParser::NODE_ELEMENT_END: {
				const String element_name = parser->get_node_name();

				if (element_name == "item") {
					inside_item = false;

					if (!current_item.title.is_empty() && !current_item.link.is_empty()) {
						news_items.push_back(current_item);
					}
				}

				current_element = String();
			} break;

			default:
				break;
		}
	}

	if (news_items.is_empty()) {
		_show_error(TTR("No news items were found."));
		return;
	}

	_show_news();
}

void ProjectManagerNews::_clear_news() {
	news_items.clear();

	if (!news_list) {
		return;
	}

	while (news_list->get_child_count() > 0) {
		Node *child = news_list->get_child(0);
		news_list->remove_child(child);
		memdelete(child);
	}
}

void ProjectManagerNews::_show_news() {
	_clear_news();

	for (const NewsItem &item : news_items) {
		_create_news_item(item);
	}

	_set_news_state(NEWS_STATE_READY);
}

void ProjectManagerNews::_create_news_item(const NewsItem &p_item) {
	PanelContainer *panel = memnew(PanelContainer);
	news_list->add_child(panel);

	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
	margin->add_theme_constant_override("margin_bottom", 12 * EDSCALE);
	panel->add_child(margin);

	VBoxContainer *container = memnew(VBoxContainer);
	container->add_theme_constant_override("separation", 6 * EDSCALE);
	margin->add_child(container);

	LinkButton *title = memnew(LinkButton);
	title->set_text(p_item.title);
	title->set_text_direction(TextServer::TEXT_DIRECTION_AUTO);
	title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	container->add_child(title);

	title->connect("pressed", ncallable_mp(this, &ProjectManagerNews::_open_news_link).bind(p_item.link));

	if (!p_item.pub_date.is_empty()) {
		Label *date = memnew(Label);
		date->set_text(p_item.pub_date);
		date->add_theme_color_override(SceneStringName(font_color), nget_theme_color("font_placeholder_color", "Editor"));
		container->add_child(date);
	}

	if (!p_item.description.is_empty()) {
		RichTextLabel *description = memnew(RichTextLabel);
		description->set_fit_content(true);
		description->set_use_bbcode(true);
		description->set_text(p_item.description);
		description->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		container->add_child(description);
	}
}

void ProjectManagerNews::_show_error(const String &p_message) {
	if (http_client) {
		memdelete(http_client);
		http_client = nullptr;
	}

	set_process(false);

	_set_news_state(NEWS_STATE_ERROR);
	status_label->set_text(p_message);
}

void ProjectManagerNews::_open_news_link(const String &p_url) {
	OS::get_singleton()->shell_open(p_url);
}

void ProjectManagerNews::_refresh_pressed() {
	refresh();
}

void ProjectManagerNews::_notification_banner_activate() {
	const int network_mode = EDITOR_GET("network/connection/network_mode");

	if (network_mode == EditorSettings::NETWORK_OFFLINE) {
		EditorSettings::get_singleton()->set_setting("network/connection/network_mode", EditorSettings::NETWORK_ONLINE);
		EditorSettings::get_singleton()->notify_changes();
		EditorSettings::get_singleton()->save();
	}

	activate();
}

void ProjectManagerNews::_notification_banner_dismiss() {
	notification_banner->hide();
}
