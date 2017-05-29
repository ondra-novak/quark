/*
 * engineState.cpp
 *
 *  Created on: 19. 5. 2017
 *      Author: ondra
 */

#include <queue>
#include "engineState.h"

#include "orderErrorException.h"
#include "core.h"


namespace quark {

std::size_t CurrentState::counter = 0;



EngineState::EngineState(Value id, json::RefCntPtr<EngineState> prevState)
	:stateId(id),prevState(prevState)

{
	if (prevState != nullptr) lastPrice = prevState->lastPrice; else lastPrice = 0;
}

}

