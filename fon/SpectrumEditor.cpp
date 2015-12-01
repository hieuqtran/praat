/* SpectrumEditor.cpp
 *
 * Copyright (C) 1992-2011,2012,2013,2014,2015 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "SpectrumEditor.h"
#include "Sound_and_Spectrum.h"
#include "EditorM.h"

Thing_implement (SpectrumEditor, FunctionEditor, 0);

#include "prefs_define.h"
#include "SpectrumEditor_prefs.h"
#include "prefs_install.h"
#include "SpectrumEditor_prefs.h"
#include "prefs_copyToInstance.h"
#include "SpectrumEditor_prefs.h"

static void updateRange (SpectrumEditor me) {
	if (Spectrum_getPowerDensityRange ((Spectrum) my data, & my minimum, & my maximum)) {
		my minimum = my maximum - my p_dynamicRange;
	} else {
		my minimum = -1000, my maximum = 1000;
	}
}

void structSpectrumEditor :: v_dataChanged () {
	updateRange (this);
	SpectrumEditor_Parent :: v_dataChanged ();
}

void structSpectrumEditor :: v_draw () {
	Spectrum spectrum = (Spectrum) our data;

	Graphics_setWindow (our d_graphics, 0, 1, 0, 1);
	Graphics_setColour (our d_graphics, Graphics_WHITE);
	Graphics_fillRectangle (our d_graphics, 0, 1, 0, 1);
	Graphics_setColour (our d_graphics, Graphics_BLACK);
	Graphics_rectangle (our d_graphics, 0, 1, 0, 1);
	Spectrum_drawInside (spectrum, our d_graphics, our d_startWindow, our d_endWindow, our minimum, our maximum);
	FunctionEditor_drawRangeMark (this, our maximum, Melder_fixed (maximum, 1), U" dB", Graphics_TOP);
	FunctionEditor_drawRangeMark (this, our minimum, Melder_fixed (minimum, 1), U" dB", Graphics_BOTTOM);
	if (our cursorHeight > our minimum && our cursorHeight < our maximum)
		FunctionEditor_drawHorizontalHair (this, our cursorHeight, Melder_fixed (our cursorHeight, 1), U" dB");
	Graphics_setColour (our d_graphics, Graphics_BLACK);

	/* Update buttons. */

	long first, last;
	long selectedSamples = Sampled_getWindowSamples (spectrum, our d_startSelection, our d_endSelection, & first, & last);
	GuiThing_setSensitive (our publishBandButton,  selectedSamples != 0);
	GuiThing_setSensitive (our publishSoundButton, selectedSamples != 0);
}

bool structSpectrumEditor :: v_click (double xWC, double yWC, bool shiftKeyPressed) {
	our cursorHeight = our minimum + yWC * (our maximum - our minimum);
	return our SpectrumEditor_Parent :: v_click (xWC, yWC, shiftKeyPressed);   // move cursor or drag selection
}

static autoSpectrum Spectrum_band (Spectrum me, double fmin, double fmax) {
	autoSpectrum band = Data_copy (me);
	double *re = band -> z [1], *im = band -> z [2];
	long imin = Sampled_xToLowIndex (band.peek(), fmin), imax = Sampled_xToHighIndex (band.peek(), fmax);
	for (long i = 1; i <= imin; i ++) re [i] = 0.0, im [i] = 0.0;
	for (long i = imax; i <= band -> nx; i ++) re [i] = 0.0, im [i] = 0.0;
	return band;
}

static autoSound Spectrum_to_Sound_part (Spectrum me, double fmin, double fmax) {
	autoSpectrum band = Spectrum_band (me, fmin, fmax);
	autoSound sound = Spectrum_to_Sound (band.peek());
	return sound;
}

void structSpectrumEditor :: v_play (double fmin, double fmax) {
	autoSound sound = Spectrum_to_Sound_part ((Spectrum) our data, fmin, fmax);
	Sound_play (sound.peek(), nullptr, nullptr);
}

static void menu_cb_publishBand (SpectrumEditor me, EDITOR_ARGS_DIRECT) {
	autoSpectrum publish = Spectrum_band ((Spectrum) my data, my d_startSelection, my d_endSelection);
	Editor_broadcastPublication (me, publish.move());
}

static void menu_cb_publishSound (SpectrumEditor me, EDITOR_ARGS_DIRECT) {
	autoSound publish = Spectrum_to_Sound_part ((Spectrum) my data, my d_startSelection, my d_endSelection);
	Editor_broadcastPublication (me, publish.move());
}

static void menu_cb_passBand (SpectrumEditor me, EDITOR_ARGS_FORM) {
	EDITOR_FORM (U"Filter (pass Hann band)", U"Spectrum: Filter (pass Hann band)...");
		REAL (U"Band smoothing (Hz)", my default_bandSmoothing ())
	EDITOR_OK
		SET_REAL (U"Band smoothing", my p_bandSmoothing)
	EDITOR_DO
		my pref_bandSmoothing() = my p_bandSmoothing = GET_REAL (U"Band smoothing");
		if (my d_endSelection <= my d_startSelection) Melder_throw (U"To apply a band-pass filter, first make a selection.");
		Editor_save (me, U"Pass band");
		Spectrum_passHannBand ((Spectrum) my data, my d_startSelection, my d_endSelection, my p_bandSmoothing);
		FunctionEditor_redraw (me);
		Editor_broadcastDataChanged (me);
	EDITOR_END
}

static void menu_cb_stopBand (SpectrumEditor me, EDITOR_ARGS_FORM) {
	EDITOR_FORM (U"Filter (stop Hann band)", nullptr)
		REAL (U"Band smoothing (Hz)", my default_bandSmoothing ())
	EDITOR_OK
		SET_REAL (U"Band smoothing", my p_bandSmoothing)
	EDITOR_DO
		my pref_bandSmoothing () = my p_bandSmoothing = GET_REAL (U"Band smoothing");
		if (my d_endSelection <= my d_startSelection) Melder_throw (U"To apply a band-stop filter, first make a selection.");
		Editor_save (me, U"Stop band");
		Spectrum_stopHannBand ((Spectrum) my data, my d_startSelection, my d_endSelection, my p_bandSmoothing);
		FunctionEditor_redraw (me);
		Editor_broadcastDataChanged (me);
	EDITOR_END
}

static void menu_cb_moveCursorToPeak (SpectrumEditor me, EDITOR_ARGS_DIRECT) {
	double frequencyOfMaximum, heightOfMaximum;
	Spectrum_getNearestMaximum ((Spectrum) my data, 0.5 * (my d_startSelection + my d_endSelection), & frequencyOfMaximum, & heightOfMaximum);
	my d_startSelection = my d_endSelection = frequencyOfMaximum;
	my cursorHeight = heightOfMaximum;
	FunctionEditor_marksChanged (me, true);
}

static void menu_cb_setDynamicRange (SpectrumEditor me, EDITOR_ARGS_FORM) {
	EDITOR_FORM (U"Set dynamic range", nullptr)
		POSITIVE (U"Dynamic range (dB)", my default_dynamicRange ())
	EDITOR_OK
		SET_REAL (U"Dynamic range", my p_dynamicRange)
	EDITOR_DO
		my pref_dynamicRange () = my p_dynamicRange = GET_REAL (U"Dynamic range");
		updateRange (me);
		FunctionEditor_redraw (me);
	EDITOR_END
}

static void menu_cb_help_SpectrumEditor (SpectrumEditor, EDITOR_ARGS_DIRECT) { Melder_help (U"SpectrumEditor"); }
static void menu_cb_help_Spectrum (SpectrumEditor, EDITOR_ARGS_DIRECT) { Melder_help (U"Spectrum"); }

void structSpectrumEditor :: v_createMenus () {
	SpectrumEditor_Parent :: v_createMenus ();
	our publishBandButton = Editor_addCommand (this, U"File", U"Publish band", 0, menu_cb_publishBand);
	our publishSoundButton = Editor_addCommand (this, U"File", U"Publish band-filtered sound", 0, menu_cb_publishSound);
	Editor_addCommand (this, U"File", U"-- close --", 0, nullptr);
	Editor_addCommand (this, U"Edit", U"-- edit band --", 0, nullptr);
	Editor_addCommand (this, U"Edit", U"Pass band...", 0, menu_cb_passBand);
	Editor_addCommand (this, U"Edit", U"Stop band...", 0, menu_cb_stopBand);
	Editor_addCommand (this, U"Select", U"-- move to peak --", 0, nullptr);
	Editor_addCommand (this, U"Select", U"Move cursor to nearest peak", 'K', menu_cb_moveCursorToPeak);
}

void structSpectrumEditor :: v_createMenuItems_view (EditorMenu menu) {
	EditorMenu_addCommand (menu, U"Set dynamic range...", 0, menu_cb_setDynamicRange);
	EditorMenu_addCommand (menu, U"-- view settings --", 0, 0);
	SpectrumEditor_Parent :: v_createMenuItems_view (menu);
}

void structSpectrumEditor :: v_createHelpMenuItems (EditorMenu menu) {
	SpectrumEditor_Parent :: v_createHelpMenuItems (menu);
	EditorMenu_addCommand (menu, U"SpectrumEditor help", '?', menu_cb_help_SpectrumEditor);
	EditorMenu_addCommand (menu, U"Spectrum help", 0, menu_cb_help_Spectrum);
}

autoSpectrumEditor SpectrumEditor_create (const char32 *title, Spectrum data) {
	try {
		autoSpectrumEditor me = Thing_new (SpectrumEditor);
		FunctionEditor_init (me.peek(), title, data);
		my cursorHeight = -1000;
		updateRange (me.peek());
		return me;
	} catch (MelderError) {
		Melder_throw (U"Spectrum window not created.");
	}
}

/* End of file SpectrumEditor.cpp */
