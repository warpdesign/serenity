/*
 * Copyright (c) 2022-2026, Undefine <undefine@undefine.pl>
 * Copyright (c) 2022-2026, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/NumberFormat.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>

static void print_cache_info(StringView description, JsonObject const& cache_object)
{
    outln("\t{}:", description);
    outln("\t\tSize: {}", human_readable_size(cache_object.get_u32("size"sv).value()));
    outln("\t\tLine size: {}", human_readable_size(cache_object.get_u32("line_size"sv).value()));
}

static void print_cpu_info(JsonObject const& value)
{
    outln("CPU {}:", value.get_u32("processor"sv).value());
    if (auto vendor = value.get_byte_string("vendor_id"sv); vendor.has_value())
        outln("\tVendor ID: {}", vendor.value());
    if (auto hypervisor = value.get_byte_string("hypervisor_vendor_id"sv); hypervisor.has_value())
        outln("\tHypervisor Vendor ID: {}", hypervisor.value());
    if (auto brand = value.get_byte_string("brand"sv); brand.has_value())
        outln("\tBrand: {}", brand.value());
    if (auto family = value.get_u32("family"sv); family.has_value())
        outln("\tFamily: {}", family.value());
    if (auto model_name = value.get_byte_string("model"sv); model_name.has_value())
        outln("\tModel: {}", model_name.value());
    else if (auto model = value.get_u32("model"sv); model.has_value())
        outln("\tModel: {}", model.value());
    if (auto stepping = value.get_u32("stepping"sv); stepping.has_value())
        outln("\tStepping: {}", stepping.value());
    if (auto type = value.get_u32("type"sv); type.has_value())
        outln("\tType: {}", type.value());
    if (auto pvr = value.get_u32("pvr"sv); pvr.has_value())
        outln("\tPVR: {:#08x}", pvr.value());

    if (auto caches = value.get_object("caches"sv); caches.has_value()) {
        if (auto cache = caches->get_object("l1_data"sv); cache.has_value())
            print_cache_info("L1 data cache"sv, cache.value());
        if (auto cache = caches->get_object("l1_instruction"sv); cache.has_value())
            print_cache_info("L1 instruction cache"sv, cache.value());
        if (auto cache = caches->get_object("l2"sv); cache.has_value())
            print_cache_info("L2 cache"sv, cache.value());
        if (auto cache = caches->get_object("l3"sv); cache.has_value())
            print_cache_info("L3 cache"sv, cache.value());
    }

    if (auto features = value.get_array("features"sv); features.has_value()) {
        out("\tFeatures: ");
        for (auto const& feature : features->values())
            out("{} ", feature.as_string());
        outln();
    }
}

ErrorOr<int> serenity_main(Main::Arguments)
{
    TRY(Core::System::pledge("stdio rpath"));

    TRY(Core::System::unveil("/sys/kernel/cpuinfo", "r"));
    TRY(Core::System::unveil(nullptr, nullptr));

    auto file = TRY(Core::File::open("/sys/kernel/cpuinfo"sv, Core::File::OpenMode::Read));
    auto file_contents = TRY(file->read_until_eof());
    auto json = TRY(JsonValue::from_string(file_contents));
    auto const& array = json.as_array();

    for (size_t i = 0; i < array.size(); i++) {
        print_cpu_info(array.at(i).as_object());
        if (i != array.size() - 1)
            outln();
    }

    return 0;
}
