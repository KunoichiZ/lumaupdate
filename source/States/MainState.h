#pragma once

#include "../State.h"

#include "../Screens/ConsoleScreen.h"
#include "../Screens/ProgressScreen.h"

class MainState : public State {
public:
	void Render();

	MainState();

private:
	enum class MainMenuState {
		Detect, //! Detecting installed version
		Fetch,  //! Fetching updated versions from internet
		Wait,   //! Waiting for user input
	};

	//! Current state of the application (loading/main menu)
	MainMenuState currentState = MainMenuState::Detect;

	//! Console screen for writing current status and debug info
	ConsoleScreen console;

	//! Progress screen for showing a pretty loading bar during detect/fetch phase
	ProgressScreen loadingScreen;
};