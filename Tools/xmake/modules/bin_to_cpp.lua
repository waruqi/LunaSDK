function bin_to_cpp(bin_file, cpp_header_file, cpp_source_file)
    local src, errmsg = io.open(bin_file, "rb")
    if src == nil then
        raise(errmsg)
    end
    local dst_header, errmsg = io.open(cpp_header_file, "w")
    if dst_header == nil then
        raise(errmsg)
    end
    local dst_source, errmsg = io.open(cpp_source_file, "w")
    if dst_source == nil then
        raise(errmsg)
    end
    errmsg = nil

    local bin_data = src:read("*a")
    src:close()
    local bin_str = ""

    for i = 1, #bin_data, 1 do
        bin_str = bin_str .. string.format("0x%02x", string.byte(bin_data, i))
        if i ~= #bin_data then
            bin_str = bin_str .. ","
        end
    end

    local bin_filename = path.filename(bin_file)
    local cpp_header_filename = path.basename(cpp_header_file)
    local cpp_source_filename = path.basename(cpp_source_file)

    local var_name = string.upper(string.gsub(bin_filename, "[\\%.| %-]", "_"))

    local header_data = "// Autogenerated by bin_to_cpp, do not modity.\n#pragma once\n#include <Runtime/Base.hpp>\n\n"
    header_data = header_data .. "namespace Luna\n{\n\textern u8 " .. var_name .. "_DATA[];\n\textern usize " .. var_name .. "_SIZE;\n}\n"

    local source_data = "// Autogenerated by bin_to_cpp, do not modity.\n#include <Runtime/Base.hpp>\n\n"
    source_data = source_data .. "namespace Luna\n{\n\tu8 " .. var_name .. "_DATA[] = {" .. bin_str .. "};\n\tusize " .. var_name .. "_SIZE = sizeof(" .. var_name .. "_DATA);\n}\n"
    
    dst_header:write(header_data)
    dst_source:write(source_data)
    dst_header:close()
    dst_source:close()
end