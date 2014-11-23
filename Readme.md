About
=====

OllyResourceRefs is a plugin for OllyDbg 2.01 that will find possible references to the resource's within the current module being debuged by OllyDbg. This is accomplished find all "push imm" commands where 'imm' is the value of a resource ID. Because some functions may have a constant as a parameter, OllyResourceRefs can only guarantee possible references to the modules resources.

Build
=====

To build OllyResourceRefs from source, checkout the latest revision and then open OllyResourceRefs.cbp with Code::Blocks and build the OllyResourceRefs project. 

Usage
=====

Copy the plugin to OllyDbg's plugin directory and once you load, or attach, OllyDbg to the module you want to debug, use the plugins menu to find possible references to resources within that module.

Double clicking on any row in the OllyResourceRefs Log window will bring you to the callers location in the OllyDbg disassembly window.

Screenshots
===========

![OllyResourceRefs Screenshot 1](https://github.com/akrutsinger/OllyResourceRefs/raw/master/screenshot1.png "OllyResourceRefs Screenshot 1")