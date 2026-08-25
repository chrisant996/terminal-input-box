// Copyright (c) 2026 Christopher Antos
// License: http://opensource.org/licenses/MIT

#include "maybe_windows.h"
#include "test.h"
#include "test_util.h"
#include "tib.h"

TEST_CASE("Key bindings")
{
    SECTION("Main")
    {
        auto base = std::make_shared<tib::key_table>();
        add_binding(*base, "\x1b[A", "command-one");
        add_binding(*base, "\x1b[B", "command-two");
        add_binding(*base, "\x1b[1~", "command-one");

        auto overlay = std::make_shared<tib::key_table>();
        add_binding(*overlay, "\x1b[A", "command-override");

        std::shared_ptr<tib::key_table_list> bindings = std::make_shared<tib::key_table_list>();
        bindings->emplace_back(base);

        std::shared_ptr<dispatcher_tester> tester = std::make_shared<dispatcher_tester>();
        tester->set_bindings(bindings);

        tib::binding_resolver resolver;

        SECTION("Base table")
        {
            assert(base->can_self_insert() <= 0);
            resolver.add_target(tester);

            {
                REQUIRE(resolver.step('\x1b').more());
                REQUIRE(resolver.step('[').more());
                auto resolved = resolver.step('A');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
                REQUIRE(resolved.binding_target);
                REQUIRE(resolved.binding_target->is_func_name("command-one"));
            }

            {
                auto resolved = resolver.step('x');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::miss);
                REQUIRE(!resolved.binding_target);
                REQUIRE(resolved.sequence == tib::cstring("x"));
            }

            {
                REQUIRE(resolver.step('\x1b').more());
                REQUIRE(resolver.step('[').more());
                auto resolved = resolver.step('Z');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::miss);
                REQUIRE(!resolved.binding_target);
                REQUIRE(resolved.sequence == tib::cstring("Z"));
            }
        }

        SECTION("Self insert")
        {
            assert(base->can_self_insert() <= 0);
            base->set_can_self_insert(true);
            resolver.add_target(tester);

            {
                REQUIRE(resolver.step('\x1b').more());
                REQUIRE(resolver.step('[').more());
                auto resolved = resolver.step('A');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
                REQUIRE(resolved.binding_target);
                REQUIRE(resolved.binding_target->is_func_name("command-one"));
            }

            {
                auto resolved = resolver.step('x');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
                REQUIRE(!resolved.binding_target);
                REQUIRE(resolved.sequence == tib::cstring("x"));
            }

            {
                REQUIRE(resolver.step('\x1b').more());
                REQUIRE(resolver.step('[').more());
                auto resolved = resolver.step('Z');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
                REQUIRE(!resolved.binding_target);
                REQUIRE(resolved.sequence == tib::cstring("Z"));
            }
        }

        SECTION("Overlay table")
        {
            assert(base->can_self_insert() <= 0);
            bindings->emplace_back(overlay);
            resolver.add_target(tester);

            {
                REQUIRE(resolver.step('\x1b').more());
                REQUIRE(resolver.step('[').more());
                auto resolved = resolver.step('A');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
                REQUIRE(resolved.binding_target);
                REQUIRE(resolved.binding_target->is_func_name("command-override"));
            }

            {
                REQUIRE(resolver.step('\x1b').more());
                REQUIRE(resolver.step('[').more());
                auto resolved = resolver.step('B');
                REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
                REQUIRE(resolved.binding_target);
                REQUIRE(resolved.binding_target->is_func_name("command-two"));
            }
        }

        SECTION("Pattern bindings")
        {
            const auto resolve_mouse = [&](const char* pattern,
                                           const char* target,
                                           const char* sequence,
                                           size_t sequence_length,
                                           std::initializer_list<const char*> expected_params) {
                REQUIRE(base->add({ pattern, tib::binding_target_func(target), true }));
                resolver.add_target(tester);

                tib::resolved_binding resolved;
                for (size_t i = 0; i < sequence_length; ++i)
                {
                    resolved = resolver.step(uint8_t(sequence[i]));
                    REQUIRE(i + 1 == sequence_length || resolved.more());
                }
                REQUIRE(resolved.outcome == tib::dispatch_outcome::match);
                REQUIRE(resolved.binding_target);
                REQUIRE(resolved.binding_target->is_func_name(target));
                REQUIRE(resolved.params.size() == expected_params.size());

                size_t i = 0;
                for (const char* expected : expected_params)
                    REQUIRE(resolved.params[i++] == tib::cstring(expected));
            };

            SECTION("VT200, default encoding")
            {
                const char sequence[] = "\x1b[M\x23\x58\x6e";
                resolve_mouse("\x1b[M%!%!%!", "vt200-default", sequence, sizeof(sequence) - 1,
                              { "3", "56", "78" });
            }

            SECTION("VT200, SGR encoding")
            {
                const char sequence[] = "\x1b[<2;56;78m";
                resolve_mouse("\x1b[<%#;%#;%#m", "vt200-sgr", sequence, sizeof(sequence) - 1,
                              { "2", "56", "78" });
            }

            SECTION("DRAG, default encoding")
            {
                const char sequence[] = "\x1b[M\x42\x20\x2a";
                resolve_mouse("\x1b[M%!%!%!", "drag-default", sequence, sizeof(sequence) - 1,
                              { "34", "0", "10" });
            }

            SECTION("DRAG, SGR encoding")
            {
                const char sequence[] = "\x1b[<34;9;10M";
                resolve_mouse("\x1b[<%#;%#;%#M", "drag-sgr", sequence, sizeof(sequence) - 1,
                              { "34", "9", "10" });
            }

            SECTION("ANY, default encoding")
            {
                const char sequence[] = { '\x1b', '[', 'M', '\x43', char(0xff), '\0' };
                resolve_mouse("\x1b[M%!%!%!", "any-default", sequence, sizeof(sequence),
                              { "35", "223", "" });
            }

            SECTION("ANY, SGR encoding")
            {
                const char sequence[] = "\x1b[<35;223;224M";
                resolve_mouse("\x1b[<%#;%#;%#M", "any-sgr", sequence, sizeof(sequence) - 1,
                              { "35", "223", "224" });
            }
        }
    }
}

TEST_CASE("UTF8 multi-byte input")
{
    SECTION("Dispatcher preserves UTF8 bytes")
    {
        REQUIRE(!tib::g_optimize_self_insert);

        auto input = std::make_shared<tib::editor_context>();
        input->initialize();
        input->set_bindings(tib::make_default_key_table());

        tib::binding_resolver resolver;
        resolver.add_target(input);

        const char utf8[] = "\xf0\x9f\x98\x80";
        for (const char c : utf8)
        {
            if (c)
            {
                auto resolved = resolver.step(c);
                REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
                resolved.dispatch();
                REQUIRE(resolved.outcome == tib::dispatch_outcome::self_insert);
            }
        }
        REQUIRE(input->get_text() == tib::cstring(utf8));
    }
}

PERF_CASE("PERF, resolve 26000 bindings")
{
    SECTION("Main")
    {
        static const char* const c_sequences[] = // 260 sequences.
        {
            // 100 one-char sequences.
            "\001", "\002", "\003", "\004", "\005", "\006", "\007", "\010", "\011", "\012",
            "\013", "\014", "\015", "\016", "\017", "\020", "\021", "\022", "\023", "\024",
            "a", "b", "c", "d", "e", "f", "g", "h", "i", "j",
            "k", "l", "m", "n", "o", "p", "q", "r", "s", "t",
            "u", "v", "w", "x", "y", "z", "[", "]", ",", ".",
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
            "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
            "U", "V", "W", "X", "Y", "Z", "{", "}", "<", ">",
            "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
            "!", "@", "#", "$", "%", "^", "&", "*", "(", ")",
            // 30 two-char sequences.
            "\033A", "\033B", "\033C", "\033D", "\033E", "\033F", "\033G", "\033H", "\033I", "\033J",
            "\033K", "\033L", "\033M", "\033N", "\033O", "\033P", "\033Q", "\033R", "\033S", "\033T",
            "\033U", "\033V", "\033W", "\033X", "\033Y", "\033Z", "\033!", "\033@", "\033#", "\033$",
            // 30 three-char sequences.
            "\033[A", "\033[B", "\033[C", "\033[D", "\033[E", "\033[F", "\033[G", "\033[H", "\033[I", "\033[J",
            "\033[K", "\033[L", "\033[M", "\033[N", "\033[O", "\033[P", "\033[Q", "\033[R", "\033[S", "\033[T",
            "\033[U", "\033[V", "\033[W", "\033[X", "\033[Y", "\033[Z", "\033[!", "\033[@", "\033[#", "\033[$",
            // 40 longer sequences.
            "\033[27;27~",
            "\033[27;1;101~",
            "\033[27;1;102~",
            "\033[27;1;103~",
            "\033[27;1;104~",
            "\033[27;1;105~",
            "\033[27;1;106~",
            "\033[27;1;107~",
            "\033[27;1;108~",
            "\033[27;1;109~",
            "\033[27;2;100~",
            "\033[27;2;101~",
            "\033[27;2;102~",
            "\033[27;2;103~",
            "\033[27;2;104~",
            "\033[27;2;105~",
            "\033[27;2;106~",
            "\033[27;2;107~",
            "\033[27;2;108~",
            "\033[27;2;109~",
            "\033[27;3;100~",
            "\033[27;3;101~",
            "\033[27;3;102~",
            "\033[27;3;103~",
            "\033[27;3;104~",
            "\033[27;3;105~",
            "\033[27;3;106~",
            "\033[27;3;107~",
            "\033[27;3;108~",
            "\033[27;3;109~",
            "\033[27;4;100~",
            "\033[27;4;101~",
            "\033[27;4;102~",
            "\033[27;4;103~",
            "\033[27;4;104~",
            "\033[27;4;105~",
            "\033[27;4;106~",
            "\033[27;4;107~",
            "\033[27;4;108~",
            "\033[27;4;109~",
            // 40 more interleaved longer sequences.
            "\033[27;3;150~",
            "\033[27;3;151~",
            "\033[27;3;152~",
            "\033[27;3;153~",
            "\033[27;3;154~",
            "\033[27;3;155~",
            "\033[27;3;156~",
            "\033[27;3;157~",
            "\033[27;3;158~",
            "\033[27;3;159~",
            "\033[27;3;160~",
            "\033[27;3;161~",
            "\033[27;3;162~",
            "\033[27;3;163~",
            "\033[27;3;164~",
            "\033[27;3;165~",
            "\033[27;3;166~",
            "\033[27;3;167~",
            "\033[27;3;168~",
            "\033[27;3;169~",
            "\033[27;3;170~",
            "\033[27;3;171~",
            "\033[27;3;172~",
            "\033[27;3;173~",
            "\033[27;3;174~",
            "\033[27;3;175~",
            "\033[27;3;176~",
            "\033[27;3;177~",
            "\033[27;3;178~",
            "\033[27;3;179~",
            "\033[27;3;180~",
            "\033[27;3;181~",
            "\033[27;3;182~",
            "\033[27;3;183~",
            "\033[27;3;184~",
            "\033[27;3;185~",
            "\033[27;3;186~",
            "\033[27;3;187~",
            "\033[27;3;188~",
            "\033[27;3;189~",
            // 10 short chord sequences.
            "\030a", "\030b", "\030c", "\030d", "\030e", "\030f", "\030g", "\030h", "\030i", "\030j",
            // 10 long chord sequences.
            "\030\033[27;1;101~",
            "\030\033[27;1;102~",
            "\030\033[27;1;103~",
            "\030\033[27;1;104~",
            "\030\033[27;1;105~",
            "\030\033[27;1;106~",
            "\030\033[27;1;107~",
            "\030\033[27;1;108~",
            "\030\033[27;1;109~",
            "\030\033[27;1;110~",
        };
        static_assert(std::size(c_sequences) == 260);

        auto table = std::make_shared<tib::key_table>();
        for (const char* sequence : c_sequences)
            REQUIRE(add_binding(*table, sequence, "command-one"));

        std::shared_ptr<tib::key_table_list> bindings = std::make_shared<tib::key_table_list>();
        bindings->emplace_back(table);

        std::shared_ptr<dispatcher_tester> tester = std::make_shared<dispatcher_tester>();
        tester->set_bindings(bindings);

        tib::binding_resolver resolver;
        resolver.add_target(tester);

        constexpr uint32_t c_passes = 100;

        uint32_t num_resolved = 0;
        for (size_t pass = 0; pass < c_passes; ++pass)
        {
            for (const char* sequence : c_sequences)
            {
                tib::dispatch_outcome outcome = tib::dispatch_outcome::miss;
                for (const char* p = sequence; *p; ++p)
                {
                    auto resolved = resolver.step(*p);
                    outcome = resolved.outcome;
                    REQUIRE(!p[1] || resolved.more());
                }
                REQUIRE(outcome == tib::dispatch_outcome::match);
                ++num_resolved;
            }
        }

        REQUIRE(num_resolved == c_passes * std::size(c_sequences));

        static_assert(c_passes * std::size(c_sequences) == 26000);
    }
}

PERF_CASE("PERF, resolve default commands 10000 times")
{
    SECTION("Main")
    {
        tib::editor_context::ensure_commands();
        const auto& commands = tib::editor_context::get_registered_commands();

        constexpr uint32_t c_passes = 10000;

        uint32_t num_resolved = 0;
        for (size_t pass = 0; pass < c_passes; ++pass)
        {
            for (const auto& command : commands)
            {
                REQUIRE(tib::editor_context::lookup_command(command.name) == command.func);
                ++num_resolved;
            }
        }

        REQUIRE(num_resolved == c_passes * commands.size());
    }
}
