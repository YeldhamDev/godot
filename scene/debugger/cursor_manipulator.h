/**************************************************************************/
/*  cursor_manipulator.h                                                  */
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

#ifndef _3D_DISABLED

#include "core/input/shortcut.h"

constexpr float DISTANCE_DEFAULT = 4;

constexpr float ZOOM_FREELOOK_MIN = 0.01;
#ifdef REAL_T_IS_DOUBLE
constexpr double ZOOM_FREELOOK_MAX = 1'000'000'000'000;
#else
constexpr float ZOOM_FREELOOK_MAX = 10'000;
#endif

class InputEvent;
class InputEventMouseMotion;
class InputEventWithModifiers;
class Viewport;

class CursorManipulator : public RefCounted {
	GDCLASS(CursorManipulator, RefCounted);

public:
	enum NavigationScheme {
		NAVIGATION_GODOT,
		NAVIGATION_MAYA,
		NAVIGATION_MODO,
		NAVIGATION_CUSTOM,
		NAVIGATION_TABLET,
	};

	enum ZoomStyle {
		ZOOM_VERTICAL,
		ZOOM_HORIZONTAL,
	};

	enum FreelookScheme {
		FREELOOK_DEFAULT,
		FREELOOK_PARTIALLY_AXIS_LOCKED,
		FREELOOK_FULLY_AXIS_LOCKED,
	};

	enum ShortcutName {
		SHORTCUT_FREELOOK_FORWARD,
		SHORTCUT_FREELOOK_BACKWARDS,
		SHORTCUT_FREELOOK_LEFT,
		SHORTCUT_FREELOOK_RIGHT,
		SHORTCUT_FREELOOK_UP,
		SHORTCUT_FREELOOK_DOWN,
		SHORTCUT_FREELOOK_SPEED_MOD,
		SHORTCUT_FREELOOK_SLOW_MOD,

		SHORTCUT_ORBIT_MOD_1,
		SHORTCUT_ORBIT_MOD_2,

		SHORTCUT_MAX,
	};

	struct Cursor {
		Vector3 pos;
		real_t x_rot, y_rot, distance, fov_scale;
		real_t unsnapped_x_rot, unsnapped_y_rot;
		Vector3 eye_pos; // Used for freelook.
		bool region_select;
		// TODO: These variables are not related to cursor manipulation, and specific
		// to Node3DEditorPlugin. So remove them in the future.
		Point2 region_begin, region_end;

		Cursor() {
			// These rotations place the camera in +X +Y +Z, aka south east, facing north west.
			x_rot = 0.5;
			y_rot = -0.5;
			unsnapped_x_rot = x_rot;
			unsnapped_y_rot = y_rot;
			distance = 4;
			fov_scale = 1.0;
			region_select = false;
		}
	};

private:
	HashMap<int, Ref<Shortcut>> inputs;

	NavigationScheme navigation_scheme = NAVIGATION_GODOT;
	ZoomStyle zoom_style = ZOOM_VERTICAL;

	bool freelook = false;
	FreelookScheme freelook_scheme = FREELOOK_DEFAULT;
	float freelook_speed = 0;
	float freelook_sensitivity = 0;

	float translation_sensitivity = 0;
	float orbit_sensitivity = 0;
	float angle_snap_threshold = 0;

	bool orthogonal = false;
	bool lock_rotation = false;

	float znear = 0;
	float zfar = 0;

	bool invert_x_axis = false;
	bool invert_y_axis = false;

	int zoom_failed_attempts_count = 0;

	bool _is_shortcut_pressed(const ShortcutName p_name);

public:
	void cursor_pan(Cursor &p_cursor, const Ref<InputEventWithModifiers> &p_event, const Vector2 &p_relative);
	void cursor_zoom(Cursor &p_cursor, const Ref<InputEventWithModifiers> p_event, const Vector2 &p_relative);
	void cursor_look(Cursor &p_cursor, const Ref<InputEventWithModifiers> &p_event, const Vector2 &p_relative);
	void cursor_orbit(Cursor &p_cursor, const Ref<InputEventWithModifiers> &p_event, const Vector2 &p_relative);

	void update_freelook(Cursor &p_cursor, const float delta);
	void scale_freelook_speed(const float p_scale);

	void scale_cursor_distance(Cursor &p_cursor, float p_scale);

	Transform3D to_camera_transform(const Cursor &p_cursor) const;

	void set_shortcut(const ShortcutName p_name, const Ref<Shortcut> &p_shortcut);

	void set_navigation_scheme(NavigationScheme p_scheme) { navigation_scheme = p_scheme; }

	void set_freelook_enabled(const bool p_enabled) { freelook = p_enabled; }
	void set_freelook_scheme(FreelookScheme p_scheme) { freelook_scheme = p_scheme; }
	void set_freelook_speed(const float p_speed) { freelook_speed = p_speed; }
	float get_freelook_speed() const { return freelook_speed; }
	void set_freelook_sensitivity(const float p_sensitivity) { freelook_sensitivity = p_sensitivity; }
	FreelookScheme get_freelook_scheme() const { return freelook_scheme; }

	void set_zoom_style(ZoomStyle p_style) { zoom_style = p_style; }

	void set_translation_sensitivity(const float p_sensitivity) { translation_sensitivity = p_sensitivity; }
	void set_orbit_sensitivity(const float p_sensitivity) { orbit_sensitivity = p_sensitivity; }
	void set_angle_snap_threshold(const float p_threshold) { angle_snap_threshold = p_threshold; }

	void set_orthogonal_mode(const bool p_enabled) { orthogonal = p_enabled; }
	void set_lock_rotation(const bool p_locked) { lock_rotation = p_locked; }
	bool is_locking_rotation() { return lock_rotation; }

	void set_z_near(const float p_near) { znear = p_near; }
	void set_z_far(const float p_far) { zfar = p_far; }

	void set_invert_x_axis(const bool p_invert) { invert_x_axis = p_invert; }
	void set_invert_y_axis(const bool p_invert) { invert_y_axis = p_invert; }

	int get_zoom_failed_attempts_count() const { return zoom_failed_attempts_count; }
};

#endif // _3D_DISABLED
