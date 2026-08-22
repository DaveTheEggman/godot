/**************************************************************************/
/*  project_manager_news.h                                                */
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

#pragma once

#include "scene/gui/box_container.h"

class Button;
class HTTPClient;
class Label;
class LinkButton;
class PanelContainer;
class RichTextLabel;
class ScrollContainer;
class VBoxContainer;

class ProjectManagerNews : public VBoxContainer {
	GDCLASS(ProjectManagerNews, VBoxContainer);

	static constexpr const char *NEWS_RSS_URL = "https://godotengine.org/rss.xml";

	struct NewsItem {
		String title;
		String link;
		String description;
		String pub_date;
	};

	enum NewsState {
		NEWS_STATE_IDLE,
		NEWS_STATE_CONNECTING,
		NEWS_STATE_LOADING,
		NEWS_STATE_ERROR,
		NEWS_STATE_READY,
	};

	NewsState news_state = NEWS_STATE_IDLE;

	HTTPClient *http_client = nullptr;

	PackedByteArray response_body;

	Vector<NewsItem> news_items;

	// Notification banner.

	PanelContainer *notification_banner = nullptr;
	Label *notification_label = nullptr;
	Button *notification_activate_button = nullptr;
	Button *notification_dismiss_button = nullptr;

	// News view.

	ScrollContainer *news_scroll = nullptr;
	VBoxContainer *news_list = nullptr;

	Label *status_label = nullptr;
	Button *refresh_button = nullptr;

	void _fetch_news();
	void _poll_http();
	void _parse_news(const PackedByteArray &p_data);
	void _clear_news();

	void _set_news_state(NewsState p_state);
	void _show_news();
	void _show_error(const String &p_message);

	void _notification_banner_activate();
	void _notification_banner_dismiss();

	void _open_news_link(const String &p_url);
	void _refresh_pressed();

	void _create_news_item(const NewsItem &p_item);

	void _update_theme();

protected:
	void _notification(int p_what);

public:
	ProjectManagerNews();
	~ProjectManagerNews();

	void activate();
	void refresh();

	bool is_loading() const;
};
