/*  Copyright (C) 2012, 2013 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

struct bear_configuration
{
    int debug;
    int dependency_generation_filtered;
    char const ** compilers;
    char const ** extensions;
};

struct bear_message;
struct bear_output;


struct bear_output * bear_open_json_output(char const * file, struct bear_configuration const * config);
void bear_close_json_output(struct bear_output * handle);

void bear_append_json_output(struct bear_output * handle, struct bear_message const * e);
