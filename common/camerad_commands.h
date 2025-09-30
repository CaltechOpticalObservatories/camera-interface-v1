/**
 * @file    camerad_commands.h
 * @brief   defines the commands accepted by the camera daemon
 * @author  David Hale <dhale@astro.caltech.edu>
 *
 */

#ifndef CAMERAD_COMMANDS_H
#define CAMERAD_COMMANDS_H
const std::string CAMERAD_CLOSE = "close";
const std::string CAMERAD_EXIT = "exit";
const std::string CAMERAD_FCS_EXPOSE = "fcs_expose";
const std::string CAMERAD_FCS_EXPTIME = "fcs_exptime";
const std::string CAMERAD_GETP = "getp";
const std::string CAMERAD_NATIVE = "native";
const std::string CAMERAD_OPEN = "open";
const std::string CAMERAD_POWER = "power";
const std::string CAMERAD_SCI_EXPOSE = "sci_expose";
const std::string CAMERAD_SCI_EXPTIME = "sci_exptime";
const std::string CAMERAD_SCI_READOUT = "sci_readout";
const std::string CAMERAD_SCI_START = "sci_start_expose";
const std::string CAMERAD_TEST = "test";

const std::vector<std::string> CAMERAD_SYNTAX = {
                                                 CAMERAD_CLOSE,
                                                 CAMERAD_EXIT,
                                                 CAMERAD_FCS_EXPOSE,
                                                 CAMERAD_FCS_EXPTIME,
                                                 CAMERAD_GETP+" [ ? ]",
                                                 CAMERAD_NATIVE+" ? | <cmd>",
                                                 CAMERAD_OPEN+" [ ? ]",
                                                 CAMERAD_POWER+" [ ? ]",
                                                 CAMERAD_SCI_EXPOSE+" [ ? ]",
                                                 CAMERAD_SCI_EXPTIME+" [ ? ]",
                                                 CAMERAD_SCI_READOUT+" [ ? ]",
                                                 CAMERAD_SCI_START+" [ ? ]",
                                                 CAMERAD_TEST+" [ ? ]"
                                               };

#endif

