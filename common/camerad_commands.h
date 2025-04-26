/**
 * @file    camerad_commands.h
 * @brief   defines the commands accepted by the camera daemon
 * @author  David Hale <dhale@astro.caltech.edu>
 * @details 
 *
 */

#pragma once

const std::string CAMERAD_ABORT("abort");
const std::string CAMERAD_AUTODIR("autodir");
const std::string CAMERAD_BASENAME("basename");
const std::string CAMERAD_BIAS("bias");
const std::string CAMERAD_BIN("bin");
const std::string CAMERAD_BOI("boi");
const std::string CAMERAD_BUFFER("buffer");
const std::string CAMERAD_CLOSE("close");
const std::string CAMERAD_CONFIG("config");
const std::string CAMERAD_ECHO("echo");
const std::string CAMERAD_EXIT("exit");
const std::string CAMERAD_EXPOSE("expose");
const std::string CAMERAD_EXPTIME("exptime");
const std::string CAMERAD_FITSNAME("fitsname");
const std::string CAMERAD_FITSNAMING("fitsnaming");
const std::string CAMERAD_FRAMETRANSFER("frametransfer");
const std::string CAMERAD_GEOMETRY("geometry");
const std::string CAMERAD_IMDIR("imdir");
const std::string CAMERAD_IMNUM("imnum");
const std::string CAMERAD_IMSIZE("imsize");
const std::string CAMERAD_INTERFACE("interface");
const std::string CAMERAD_ISOPEN("isopen");
const std::string CAMERAD_KEY("key");
const std::string CAMERAD_LOAD("load");
const std::string CAMERAD_LONGERROR("longerror");
const std::string CAMERAD_MEX("mex");
const std::string CAMERAD_MEXAMPS("mexamps");
const std::string CAMERAD_MODEXPTIME("modexptime");
const std::string CAMERAD_NATIVE("native");
const std::string CAMERAD_OPEN("open");  const int CAMERAD_OPEN_TIMEOUT(10000);
const std::string CAMERAD_PAUSE("pause");
const std::string CAMERAD_POWER("power");
const std::string CAMERAD_PREEXPOSURES("preexposures");
const std::string CAMERAD_READOUT("readout");
const std::string CAMERAD_RESUME("resume");
const std::string CAMERAD_SHUTTER("shutter");
const std::string CAMERAD_STOP("stop");
const std::string CAMERAD_TEST("test");
const std::string CAMERAD_USEFRAMES("useframes");
const std::string CAMERAD_WRITEKEYS("writekeys");
const std::vector<std::string> CAMERAD_SYNTAX = {
                                                  CAMERAD_ABORT,
                                                  CAMERAD_AUTODIR,
                                                  CAMERAD_BASENAME,
                                                  CAMERAD_BIAS,
                                                  CAMERAD_BIN+" ? | <axis> [ <binfactor> ]",
                                                  CAMERAD_BOI+" ?|<chan>|<dev#> [full|<nskip1> <nread1> [<nskip2> <nread2> [...]]]]",
                                                  CAMERAD_BUFFER+" ? | <dev#> | <chan> [ <bytes> | <cols> <rows> ]",
                                                  CAMERAD_CLOSE,
                                                  CAMERAD_CONFIG,
                                                  CAMERAD_ECHO,
                                                  CAMERAD_EXPOSE,
                                                  CAMERAD_EXIT,
                                                  CAMERAD_EXPTIME+" [ <exptime in msec> ]",
                                                  CAMERAD_FITSNAME,
                                                  CAMERAD_FITSNAMING+" [ time | number ]",
                                                  CAMERAD_FRAMETRANSFER+" ? | <dev#> | <chan> [ yes | no ]",
                                                  CAMERAD_GEOMETRY+" ? | <dev#> | <chan> [ <bytes> | <cols> <rows> ]",
                                                  CAMERAD_IMDIR+" [ <directory> ]",
                                                  CAMERAD_IMNUM,
                                                  CAMERAD_IMSIZE+" ? | <dev#>|<chan> [ <cols> <rows> <oscols> <osrows> ]",
                                                  CAMERAD_INTERFACE,
                                                  CAMERAD_ISOPEN,
                                                  CAMERAD_KEY,
                                                  CAMERAD_LOAD,
                                                  CAMERAD_LONGERROR,
                                                  CAMERAD_MEX,
                                                  CAMERAD_MEXAMPS,
                                                  CAMERAD_MODEXPTIME,
                                                  CAMERAD_NATIVE+" ? | [<DEV>|<CHAN>] <CMD> [ <ARG1> [ < ARG2> [ <ARG3> [ <ARG4> ] ] ] ]",
                                                  CAMERAD_OPEN+" [ ? | <devlist> ]",
                                                  CAMERAD_PAUSE,
                                                  CAMERAD_PREEXPOSURES,
                                                  CAMERAD_READOUT+" [ ? ] | [ <dev#> | <chan> [ <amp> ] ]",
                                                  CAMERAD_RESUME,
                                                  CAMERAD_SHUTTER+" [ ? | enable | 1 | disable | 0 ]",
                                                  CAMERAD_STOP,
                                                  CAMERAD_TEST+" ? | <testname> ...",
                                                  CAMERAD_USEFRAMES,
                                                  CAMERAD_WRITEKEYS
                                                };
