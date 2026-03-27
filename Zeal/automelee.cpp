#include "automelee.h"

#include "callbacks.h"

#include "zeal.h"

void AutoMelee::tick() {}

AutoMelee::AutoMelee(ZealService *zeal) {

  zeal->callbacks->AddGeneric([this]() { tick(); });
}

AutoMelee::~AutoMelee() {}