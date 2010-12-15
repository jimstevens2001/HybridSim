#!/bin/sh

emacs TraceBasedSim.cpp HybridSystem.cpp --eval '(delete-other-windows)'&

emacs TraceBasedSim.h HybridSystem.h HybridSim.h FDSim.h config.h CallbackHybrid.h --eval '(delete-other-windows)'&

echo opening files