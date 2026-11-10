#include "../Sources/Native/XRayBootstrap.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace stalker;
    XRayBootstrap engine;
    assert(engine.state() == EngineState::cold);
    assert(!engine.attachResources("."));
    assert(engine.initialize(GameEdition::pripyat));
    assert(engine.edition() == GameEdition::pripyat);
    assert(!engine.initialize(GameEdition::shadow));
    assert(!engine.attachResources("/definitely/not/a/game/folder"));
    assert(engine.attachResources("."));
    assert(engine.state() == EngineState::resourcesReady);
    assert(!engine.attachResources("."));
    engine.shutdown();
    assert(engine.state() == EngineState::cold);
    assert(engine.resourceRoot().empty());
    assert(engine.initialize(GameEdition::shadow));
    std::cout << "X-Ray bootstrap lifecycle tests passed\n";
}
