#pragma once

// Variant policy for agent/envdiffuse-stock-dsr-20261020.
// This branch intentionally preserves the stock DSR EnvDiffuse consumer.
// The define is source-owned rather than an optional runtime toggle so the
// source and integrated addon cannot accidentally ship with donor-required
// EnvDiffuse behavior after a build-flag omission.
#ifndef DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE
#define DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE 1
#endif
