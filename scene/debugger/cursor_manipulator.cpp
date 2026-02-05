/**************************************************************************/
/*  cursor_manipulator.cpp                                                */
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

#ifndef _3D_DISABLED

#include "cursor_manipulator.h"

#include "core/input/input.h"
#include "core/input/input_event.h"

void CursorManipulator::cursor_pan(Cursor &p_cursor, const Ref<InputEventWithModifiers> &p_event, const Vector2 &p_relative) {
	float pan_speed = translation_sensitivity / 150.0;
	if (p_event.is_valid() && navigation_scheme == NAVIGATION_MAYA && p_event->is_shift_pressed()) {
		pan_speed *= 10;
	}

	Transform3D camera_transform;

	camera_transform.translate_local(p_cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -p_cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -p_cursor.y_rot);
	Vector3 translation(
			(invert_x_axis ? -1 : 1) * -p_relative.x * pan_speed,
			(invert_y_axis ? -1 : 1) * p_relative.y * pan_speed,
			0);
	translation *= p_cursor.distance / DISTANCE_DEFAULT;
	camera_transform.translate_local(translation);
	p_cursor.pos = camera_transform.origin;
}

void CursorManipulator::cursor_orbit(Cursor &p_cursor, const Ref<InputEventWithModifiers> &p_event, const Vector2 &p_relative) {
	if (lock_rotation) {
		cursor_pan(p_cursor, p_event, p_relative);
		return;
	}

	const float radians_per_pixel = Math::deg_to_rad(orbit_sensitivity);

	p_cursor.unsnapped_x_rot += p_relative.y * radians_per_pixel * (invert_y_axis ? -1 : 1);
	p_cursor.unsnapped_x_rot = CLAMP(p_cursor.unsnapped_x_rot, -1.57, 1.57);
	p_cursor.unsnapped_y_rot += p_relative.x * radians_per_pixel * (invert_x_axis ? -1 : 1);

	p_cursor.x_rot = p_cursor.unsnapped_x_rot;
	p_cursor.y_rot = p_cursor.unsnapped_y_rot;

	if (_is_shortcut_pressed(SHORTCUT_ORBIT_MOD_1) || _is_shortcut_pressed(SHORTCUT_ORBIT_MOD_2)) {
		const float snap_angle = Math::deg_to_rad(45.0);
		const float snap_threshold = Math::deg_to_rad(angle_snap_threshold);

		float x_rot_snapped = Math::snapped(p_cursor.unsnapped_x_rot, snap_angle);
		float y_rot_snapped = Math::snapped(p_cursor.unsnapped_y_rot, snap_angle);

		float x_dist = Math::abs(p_cursor.unsnapped_x_rot - x_rot_snapped);
		float y_dist = Math::abs(p_cursor.unsnapped_y_rot - y_rot_snapped);

		if (x_dist < snap_threshold && y_dist < snap_threshold) {
			p_cursor.x_rot = x_rot_snapped;
			p_cursor.y_rot = y_rot_snapped;
		}
	}
}

void CursorManipulator::cursor_look(Cursor &p_cursor, const Ref<InputEventWithModifiers> &p_event, const Vector2 &p_relative) {
	if (orthogonal) {
		cursor_pan(p_cursor, p_event, p_relative);
		return;
	}

	// Scale mouse sensitivity with camera FOV scale when zoomed in to make it easier to point at things.
	const float degrees_per_pixel = freelook_sensitivity * MIN(1.0, p_cursor.fov_scale);
	const float radians_per_pixel = Math::deg_to_rad(degrees_per_pixel);

	// Note: do NOT assume the camera has the "current" transform, because it is interpolated and may have "lag".
	const Transform3D prev_camera_transform = to_camera_transform(p_cursor);

	if (invert_y_axis) {
		p_cursor.x_rot -= p_relative.y * radians_per_pixel;
	} else {
		p_cursor.x_rot += p_relative.y * radians_per_pixel;
	}

	// Clamp the Y rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
	p_cursor.x_rot = CLAMP(p_cursor.x_rot, -1.57, 1.57);
	p_cursor.unsnapped_x_rot = p_cursor.x_rot;

	p_cursor.y_rot += p_relative.x * radians_per_pixel;
	p_cursor.unsnapped_y_rot = p_cursor.y_rot;

	// Look is like the opposite of Orbit: the focus point rotates around the camera
	Transform3D camera_transform = to_camera_transform(p_cursor);
	Vector3 pos = camera_transform.xform(Vector3(0, 0, 0));
	Vector3 prev_pos = prev_camera_transform.xform(Vector3(0, 0, 0));
	Vector3 diff = prev_pos - pos;
	p_cursor.pos += diff;
}

void CursorManipulator::cursor_zoom(Cursor &p_cursor, const Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative) {
	float zoom_speed = 1 / 80.0;
	if (p_event.is_valid() && navigation_scheme == NAVIGATION_MAYA && p_event->is_shift_pressed()) {
		zoom_speed *= 10;
	}

	if (zoom_style == ZOOM_HORIZONTAL) {
		if (p_relative.x > 0) {
			scale_cursor_distance(p_cursor, 1 - p_relative.x * zoom_speed);
		} else if (p_relative.x < 0) {
			scale_cursor_distance(p_cursor, 1.0 / (1 + p_relative.x * zoom_speed));
		}
	} else {
		if (p_relative.y > 0) {
			scale_cursor_distance(p_cursor, 1 + p_relative.y * zoom_speed);
		} else if (p_relative.y < 0) {
			scale_cursor_distance(p_cursor, 1.0 / (1 - p_relative.y * zoom_speed));
		}
	}
}

bool CursorManipulator::_is_shortcut_pressed(const ShortcutName p_name) {
	Ref<Shortcut> shortcut = inputs[p_name];
	if (shortcut.is_null()) {
		return false;
	}

	const Array shortcuts = shortcut->get_events();
	Ref<InputEventKey> k;
	if (shortcuts.size() > 0) {
		k = shortcuts.front();
	}

	if (k.is_null()) {
		return false;
	}

	if (k->get_physical_keycode() == Key::NONE) {
		return Input::get_singleton()->is_key_pressed(k->get_keycode());
	}
	return Input::get_singleton()->is_physical_key_pressed(k->get_physical_keycode());
}

void CursorManipulator::update_freelook(Cursor &p_cursor, const float delta) {
	if (!freelook) {
		return;
	}

	const Transform3D camera_transform = to_camera_transform(p_cursor);

	Vector3 forward;
	if (freelook_scheme == FREELOOK_FULLY_AXIS_LOCKED) {
		// Forward/backward keys will always go straight forward/backward, never moving on the Y axis.
		forward = Vector3(0, 0, -1).rotated(Vector3(0, 1, 0), camera_transform.get_basis().get_euler().y);
	} else {
		// Forward/backward keys will be relative to the camera pitch.
		forward = camera_transform.basis.xform(Vector3(0, 0, -1));
	}

	const Vector3 right = camera_transform.basis.xform(Vector3(1, 0, 0));

	Vector3 up;
	if (freelook_scheme == CursorManipulator::FREELOOK_PARTIALLY_AXIS_LOCKED || freelook_scheme == CursorManipulator::FREELOOK_FULLY_AXIS_LOCKED) {
		// Up/down keys will always go up/down regardless of camera pitch.
		up = Vector3(0, 1, 0);
	} else {
		// Up/down keys will be relative to the camera pitch.
		up = camera_transform.basis.xform(Vector3(0, 1, 0));
	}

	Vector3 direction;
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_LEFT)) {
		direction -= right;
	}
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_RIGHT)) {
		direction += right;
	}
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_FORWARD)) {
		direction += forward;
	}
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_BACKWARDS)) {
		direction -= forward;
	}
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_UP)) {
		direction += up;
	}
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_DOWN)) {
		direction -= up;
	}

	real_t speed = freelook_speed;

	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_SPEED_MOD)) {
		speed *= 3.0;
	}
	if (_is_shortcut_pressed(SHORTCUT_FREELOOK_SLOW_MOD)) {
		speed *= 0.333333;
	}

	const Vector3 motion = direction * speed * delta;
	p_cursor.pos += motion;
	p_cursor.eye_pos += motion;
}

void CursorManipulator::scale_freelook_speed(const float p_scale) {
	float min_speed = MAX(znear * 4, ZOOM_FREELOOK_MIN);
	float max_speed = MIN(zfar / 4, ZOOM_FREELOOK_MAX);
	if (unlikely(min_speed > max_speed)) {
		freelook_speed = (min_speed + max_speed) / 2;
	} else {
		freelook_speed = CLAMP(freelook_speed * p_scale, min_speed, max_speed);
	}
}

void CursorManipulator::scale_cursor_distance(Cursor &p_cursor, const float p_scale) {
	float min_distance = MAX(znear * 4, ZOOM_FREELOOK_MIN);
	float max_distance = MIN(zfar / 4, ZOOM_FREELOOK_MAX);
	if (unlikely(min_distance > max_distance)) {
		p_cursor.distance = (min_distance + max_distance) / 2;
	} else {
		p_cursor.distance = CLAMP(p_cursor.distance * p_scale, min_distance, max_distance);
	}

	if (p_cursor.distance == max_distance || p_cursor.distance == min_distance) {
		zoom_failed_attempts_count++;
	} else {
		zoom_failed_attempts_count = 0;
	}
}

Transform3D CursorManipulator::to_camera_transform(const Cursor &p_cursor) const {
	Transform3D camera_transform;
	camera_transform.translate_local(p_cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -p_cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -p_cursor.y_rot);

	if (orthogonal) {
		camera_transform.translate_local(0, 0, (zfar - znear) / 2.0);
	} else {
		camera_transform.translate_local(0, 0, p_cursor.distance);
	}

	return camera_transform;
}

void CursorManipulator::set_shortcut(const ShortcutName p_name, const Ref<Shortcut> &p_shortcut) {
	ERR_FAIL_INDEX(0, SHORTCUT_MAX);
	ERR_FAIL_COND(p_shortcut.is_null());
	inputs[p_name] = p_shortcut;
}

#endif // _3D_DISABLED
