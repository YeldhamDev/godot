/**************************************************************************/
/*  embedded_process.cpp                                                  */
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

#include "embedded_process.h"
#include "scene/main/window.h"
#include "scene/theme/theme_db.h"

void EmbeddedProcess::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_window = get_window();
		} break;
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_VISIBILITY_CHANGED:
		case NOTIFICATION_WM_POSITION_CHANGED: {
			_update_embedded_process();
		} break;
	}
}

void EmbeddedProcess::set_embedding_timeout(int p_timeout) {
	_embedding_timeout = p_timeout;
}

int EmbeddedProcess::get_embedding_timeout() {
	return _embedding_timeout;
}

void EmbeddedProcess::set_window_size(Size2i p_window_size) {
	_window_size = p_window_size;
	_update_embedded_process();
}

Size2i EmbeddedProcess::get_window_size() {
	return _window_size;
}

void EmbeddedProcess::set_keep_aspect(bool p_keep_aspect) {
	_keep_aspect = p_keep_aspect;
	_update_embedded_process();
}

bool EmbeddedProcess::get_keep_aspect() {
	return _keep_aspect;
}

Rect2i EmbeddedProcess::get_global_embedded_window_rect() {
	Rect2i control_rect = this->get_global_rect();
	if (control_rect.size == Size2i()) {
		// The control is probably not visible. We will spawn the window anyway
		// at its "normal" size. It will not be visible regardless
		// because embed_process should be called with p_visible set to false.
		control_rect = Rect2i(_window_size.x, 0, _window_size.x, _window_size.y);
	}
	if (_keep_aspect) {
		Rect2i desired_rect = control_rect;
		float ratio = MIN((float)control_rect.size.x / _window_size.x, (float)control_rect.size.y / _window_size.y);
		desired_rect.size = Size2i(_window_size.x * ratio, _window_size.y * ratio);
		desired_rect.position = Size2i(control_rect.position.x + ((control_rect.size.x - desired_rect.size.x) / 2), control_rect.position.y + ((control_rect.size.y - desired_rect.size.y) / 2));
		return desired_rect;
	} else {
		return control_rect;
	}
}

Rect2i EmbeddedProcess::get_screen_embedded_window_rect() {
	if (_keep_aspect) {
		Rect2i rect = get_global_embedded_window_rect();
		rect.position = get_screen_position() + (rect.position - get_global_position());
		return rect;
	} else {
		return this->get_screen_rect();
	}
}

bool EmbeddedProcess::is_embedding_in_progress() {
	return !timer_embedding->is_stopped();
}

bool EmbeddedProcess::is_embedding_completed() {
	return _embedding_completed;
}

void EmbeddedProcess::embed_process(OS::ProcessID p_pid) {
	if (!_window) {
		return;
	}

	ERR_FAIL_COND_MSG(!DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_WINDOW_EMBEDDING), "Embedded process not supported by this display server.");

	if (_current_process_id != 0) {
		// Stop embedding the last process.
		OS::get_singleton()->kill(_current_process_id);
	}

	reset();

	_current_process_id = p_pid;
	_start_embedding_time = OS::get_singleton()->get_ticks_msec();

	// Try to embed, but the process may be just started and the window is not yet ready
	// we will retry in this case.
	_try_embed_process();
}

void EmbeddedProcess::reset() {
	if (_current_process_id != 0 && _embedding_completed) {
		DisplayServer::get_singleton()->remove_embedded_process(_current_process_id);
	}
	_current_process_id = 0;
	_embedding_completed = false;
	_start_embedding_time = 0;
	timer_embedding->stop();
}

void EmbeddedProcess::_try_embed_process() {
	Error err = DisplayServer::get_singleton()->embed_process(_window->get_window_id(), _current_process_id, get_screen_embedded_window_rect(), is_visible_in_tree());
	if (err == OK) {
		_embedding_completed = true;
		emit_signal(SNAME("embedding_completed"));
	} else if (err == ERR_DOES_NOT_EXIST) {
		if (OS::get_singleton()->get_ticks_msec() - _start_embedding_time >= (uint64_t)_embedding_timeout) {
			// Embedding failed.
			reset();
			emit_signal(SNAME("embedding_failed"));
		} else {
			// Tries another shot.
			timer_embedding->start();
		}
	} else {
		// Another error.
		reset();
		emit_signal(SNAME("embedding_failed"));
	}
}

void EmbeddedProcess::_update_embedded_process() {
	if (!_window || _current_process_id == 0 || !_embedding_completed) {
		return;
	}

	DisplayServer::get_singleton()->embed_process(_window->get_window_id(), _current_process_id, get_screen_embedded_window_rect(), is_visible_in_tree());
}

void EmbeddedProcess::_timer_embedding_timeout() {
	_try_embed_process();
}

void EmbeddedProcess::_bind_methods() {
	ClassDB::bind_method(D_METHOD("embed_process", "process_id"), &EmbeddedProcess::embed_process);
	ClassDB::bind_method(D_METHOD("reset"), &EmbeddedProcess::reset);
	ClassDB::bind_method(D_METHOD("set_embedding_timeout", "timeout"), &EmbeddedProcess::set_embedding_timeout);
	ClassDB::bind_method(D_METHOD("get_embedding_timeout"), &EmbeddedProcess::get_embedding_timeout);
	ClassDB::bind_method(D_METHOD("is_embedding_completed"), &EmbeddedProcess::is_embedding_completed);
	ClassDB::bind_method(D_METHOD("is_embedding_in_progress"), &EmbeddedProcess::is_embedding_in_progress);

	ADD_SIGNAL(MethodInfo("embedding_completed"));
	ADD_SIGNAL(MethodInfo("embedding_failed"));
}

EmbeddedProcess::EmbeddedProcess() {
	timer_embedding = memnew(Timer);
	timer_embedding->set_wait_time(0.1);
	timer_embedding->set_one_shot(true);
	add_child(timer_embedding);
	timer_embedding->connect("timeout", callable_mp(this, &EmbeddedProcess::_timer_embedding_timeout));
}

EmbeddedProcess::~EmbeddedProcess() {
	if (_current_process_id != 0) {
		// Stop embedding the last process.
		OS::get_singleton()->kill(_current_process_id);
		reset();
	}
}
