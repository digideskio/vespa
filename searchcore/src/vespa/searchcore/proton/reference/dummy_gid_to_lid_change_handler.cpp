// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "dummy_gid_to_lid_change_handler.h"

namespace proton {

DummyGidToLidChangeHandler::DummyGidToLidChangeHandler()
    : IGidToLidChangeHandler()

{
}

DummyGidToLidChangeHandler::~DummyGidToLidChangeHandler()
{
}

void
DummyGidToLidChangeHandler::notifyPutDone(GlobalId, uint32_t, SerialNum)
{
}

void
DummyGidToLidChangeHandler::notifyRemove(GlobalId, SerialNum)
{
}

void
DummyGidToLidChangeHandler::notifyRemoveDone(GlobalId, SerialNum)
{
}

void
DummyGidToLidChangeHandler::addListener(std::unique_ptr<IGidToLidChangeListener>)
{
}

void
DummyGidToLidChangeHandler::removeListeners(const vespalib::string &,
                                            const std::set<vespalib::string> &)
{
}

} // namespace proton
