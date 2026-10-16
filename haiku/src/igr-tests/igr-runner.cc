#include <string>
#include <iostream>
#include <cstring>

#include <dirent.h>
#include <pwd.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "igr.h"
#include "mconfig.h"

// Integration test suite runner.

void basic_test();
void environ_test();
void environ2_test();
void ps_environ_test();
void chain_to_test();
void force_stop_test();
void restart_test();
void check_basic_test();
void check_cycle_test();
void check_cycle2_test();
void check_lint_test();
void check_consumer_of_test();
void reload1_test();
void reload2_test();
void no_command_error_test();
void add_rm_dep_test();
void var_subst_test();
void svc_start_fail_test();
void dep_not_found_test();
void pseudo_cycle_test();
void before_after_test();
void before_after2_test();
void log_via_pipe_test();
void catlog_test();
void offline_enable_test();
void xdg_config_test();
void cycles_test();
void svc_arg_test();
void svc_arg_consumer_test();
void svc_arg_enable_test();

int main(int argc, char **argv)
{
    struct test tests[] = { { "basic", basic_test }, { "environ", environ_test },
            { "environ2", environ2_test }, { "ps-environ", ps_environ_test },
            { "chain-to", chain_to_test }, { "force-stop", force_stop_test },
            { "restart", restart_test }, { "check-basic", check_basic_test },
            { "check-cycle", check_cycle_test }, { "check-cycle2", check_cycle2_test },
            { "check-lint", check_lint_test },
            { "check-consumer-of", check_consumer_of_test }, { "reload1", reload1_test },
            { "reload2", reload2_test }, { "no-command-error", no_command_error_test },
            { "add-rm-dep", add_rm_dep_test }, { "var-subst", var_subst_test },
            { "svc-start-fail", svc_start_fail_test, }, { "dep-not-found", dep_not_found_test },
            { "pseudo-cycle", pseudo_cycle_test }, { "before-after", before_after_test },
            { "before-after2", before_after2_test }, { "log-via-pipe", log_via_pipe_test },
            { "catlog", catlog_test }, { "offline-enable", offline_enable_test },
            { "xdg-config", xdg_config_test }, { "cycles", cycles_test },
            { "svc-arg", svc_arg_test }, { "svc-arg-consumer", svc_arg_consumer_test },
            { "svc-arg-enable", svc_arg_enable_test } };

    haiku_bindir = "../..";
    char *env_haiku_bindir = getenv("HAIKU_BINDIR");
    if (env_haiku_bindir != nullptr) {
        haiku_bindir = env_haiku_bindir;
    }

    igr_output_basedir = "igr-output";
    char *env_igr_output_base = getenv("IGR_OUTPUT_BASE");
    if (env_igr_output_base != nullptr) {
        igr_output_basedir = env_igr_output_base;
    }

    igr_input_basedir = get_full_cwd();
    char *env_igr_input_basedir = getenv("IGR_INPUT_BASE");
    if (env_igr_input_basedir != nullptr) {
        igr_input_basedir = env_igr_input_basedir;
    }

    int passed = 0;
    int failed = 0;

    // Figure out the maximum width (with +2 buffer)
    size_t max_width = 0;
    for (test &t : tests) {
        max_width = std::max(max_width, strlen(t.name) + 2);
    }

    std::cout << "============== INTEGRATION TESTS =====================" << std::endl;

    if (mkdir(igr_output_basedir.c_str(), 0700) == -1 && errno != EEXIST) {
        throw std::system_error(errno, std::generic_category(), std::string("mkdir: ") + igr_output_basedir);
    }

    // A single test can be requested through single argument
    if (argc == 2) {
        for (test test : tests) {
            if (strcmp(test.name, argv[1]) == 0) {
                std::string ellipsis(max_width - strlen(test.name), '.');
                std::cout << test.name << ellipsis << " " << std::flush;
                try {
                    test.func();
                    std::cout << "PASSED" << std::endl;
                }
                catch (igr_failure_exc &exc) {
                    std::cout << exc.get_message() << std::endl;
                    std::cout << "FAILED" << std::endl;
                    return 1;
                }
                return 0;
            }
        }
        std::cerr << "Couldn't find the test: " << argv[1] << std::endl;
        return 1;
    }

    for (test test : tests) {
        std::string ellipsis(max_width - strlen(test.name), '.');
        std::cout << test.name << ellipsis << " " << std::flush;

        bool success;
        std::string failure_msg;
        try {
            test.func();
            success = true;
        }
        catch (igr_failure_exc &exc) {
            success = false;
            failure_msg = exc.get_message();
        }

        if (success) {
            std::cout << "PASSED\n";
            passed++;
        }
        else {
            std::cout << "FAILED\n";
            failed++;
            std::cout << failure_msg << std::endl;
        }

        continue;
    }

    std::cout << "======================================================" << std::endl;

    std::cout << "Test run finished.\n"
            "Passed: " << passed << "\n"
            "Failed: " << failed;
    if (failed != 0) {
        std::cout << " XXX";
    }

    std::cout << std::endl;

    return failed == 0 ? 0 : 1;
}

// Recurse through files in a directory tree, processing each one via the supplied function.
template <typename process_func_t>
void recurse_tree(const char *path, process_func_t process_func)
{
    DIR *dir_handle = opendir(path);
    if (dir_handle == nullptr) {
        throw igr_failure_exc(std::string("recurse_tree: opendir failed for: ") + path);
    }

    struct dirent *dir_entry;
    while ((dir_entry = readdir(dir_handle)) != nullptr) {
        if (dir_entry->d_name[0] == '.' && (dir_entry->d_name[1] == '\0' ||
                (dir_entry->d_name[1] == '.' && dir_entry->d_name[2] == '\0'))) {
            // skip "." and ".." entries
            continue;
        }

        struct stat statbuf;
        std::string full_path = path;
        full_path += '/';
        full_path += dir_entry->d_name;
        if (lstat(full_path.c_str(), &statbuf) != 0) {
            throw igr_failure_exc("recurse_tree: lstat failed for: " + full_path);
        }
        if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
            recurse_tree(full_path.c_str(), process_func);
        }

        process_func(full_path, statbuf);
    }

    closedir(dir_handle);
}

// Recursively remove a directory (a la "rm -r")
void rm_tree(const char *path)
{
    recurse_tree(path, [&](const std::string &filepath, const struct stat &statbuf) {
        if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
            int r = rmdir(filepath.c_str());
            if (r != 0) {
                throw igr_failure_exc("rm_tree: rmdir failed for " + filepath + ": " + strerror(errno));
            }
        }
        else {
            int r= unlink(filepath.c_str());
            if (r != 0) {
                throw igr_failure_exc("rm_tree: unlink failed for " + filepath + ": " + strerror(errno));
            }
        }
    });
}

void basic_test()
{
    igr_test_setup setup("basic");
    std::string ran_file = setup.prep_output_file("basic-ran");
    std::string socket_path = setup.prep_socket_path();

    // Start the "basic" service. This creates an output file, "basic-ran", containing "ran\n".
    haiku_proc haiku_p;
    haiku_p.start("basic", {"-u", "-d", "sd", "-p", socket_path, "-q", "basic"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    igr_assert_eq("", haiku_p.get_stdout());
    igr_assert_eq("", haiku_p.get_stderr());

    check_file_contents(ran_file.c_str(), "ran\n");
}

void environ_test()
{
    igr_test_setup setup("environ");
    std::string output_file = setup.prep_output_file("env-record");
    std::string socket_path = setup.prep_socket_path();

    igr_env_var_setup env_output("OUTPUT", output_file.c_str());
    igr_env_var_setup env_socket("SOCKET", socket_path.c_str());
    igr_env_var_setup env_haikuctl("HAIKUCTL", (haiku_bindir + "/haikuctl").c_str());
    igr_env_var_setup env_bar("BAR", "to_be_unset");

    haiku_proc haiku_p;
    haiku_p.start("environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "-e", "environment1", "checkenv"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    haiku_p.start("environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "-e", "environment2", "checkenv"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    haiku_p.start("environ", {"-u", "-d", "sd", "-p", socket_path, /* "-q", */ "setenv1"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    check_file_contents(output_file, socket_path + "\n" +
            "checkenv\n" +
            "gotenv1\n" +
            "hello\n" +
            "gotenv2\n" +
            "goodbye\n" +
            "3\n2\n1\n");
}

void environ2_test()
{
    igr_test_setup setup("environ2");
    std::string output_file = setup.prep_output_file("env-record");
    std::string socket_path = setup.prep_socket_path();

    uid_t my_uid = getuid();
    gid_t my_gid = getgid();
    struct passwd *my_pwd_ent = getpwuid(my_uid);
    if (my_pwd_ent == nullptr) {
        throw std::system_error(errno, std::generic_category(), "getpwuid");
    }

    // unset variables to make sure the values seen in the test service were initialised by haiku:
    igr_env_var_setup env_user("USER", nullptr);
    igr_env_var_setup env_logname("LOGNAME", nullptr);
    igr_env_var_setup env_shell("SHELL", nullptr);
    igr_env_var_setup env_uid("UID", nullptr);
    igr_env_var_setup env_gid("GID", nullptr);

    // test whether vars from global environment propagate
    igr_env_var_setup env_test_var("TEST_VAR", "helloworld");

    haiku_proc haiku_p;
    haiku_p.start("environ2", {"-u", "-d", "sd", "-p", socket_path, "-q", "-e", "env-haiku", "checkenv"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    check_file_contents(output_file,
            std::string("helloworld\n") +
            "hello\n" +
            "override\n" +
            my_pwd_ent->pw_name + "\n" +
            my_pwd_ent->pw_name + "\n" +
            "/bogus/value\n" +
            std::to_string(my_uid) + "\n" +
            std::to_string(my_gid) + "\n");
}

void ps_environ_test()
{
    igr_test_setup setup("ps-environ");
    std::string output_file = setup.prep_output_file("env-record");
    std::string socket_path = setup.prep_socket_path();

    igr_env_var_setup env_output("OUTPUT", output_file.c_str());
    igr_env_var_setup env_test_var_two("TEST_VAR_TWO", "set-via-script");

    haiku_proc haiku_p;
    haiku_p.start("ps-environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "checkenv1"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    haiku_p.start("ps-environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "checkenv2"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    haiku_p.start("ps-environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "checkenv3"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    // "set-in-haiku-env"
    haiku_p.start("ps-environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "-e", "haiku-environment", "checkenv4"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    // "set-via-script" (as per above)
    haiku_p.start("ps-environ", {"-u", "-d", "sd", "-p", socket_path, "-q", "checkenv4"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    check_file_contents(output_file, read_file_contents(igr_input_basedir + "/ps-environ/env-expected"));
}

void chain_to_test()
{
    igr_test_setup setup("chain-to");
    std::string output_file = setup.prep_output_file("recorded-output");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("chain-to", {"-u", "-d", "sd", "-p", socket_path, "-q", "part1"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    check_file_contents(output_file, read_file_contents(igr_input_basedir + "/chain-to/expected-output"));
}

void force_stop_test()
{
    igr_test_setup setup("force-stop");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("force-stop", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    // "haikuctl list"
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("force-stop", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq(read_file_contents(igr_input_basedir + "/force-stop/expected-1"), haikuctl_p.get_stdout());
    igr_assert_eq("", haikuctl_p.get_stderr());

    // "haikuctl stop critical"
    haikuctl_p.start("force-stop", {"-p", socket_path, "stop", "critical"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq("", haikuctl_p.get_stdout());
    igr_assert_eq(read_file_contents(igr_input_basedir + "/force-stop/expected-2.err"), haikuctl_p.get_stderr());

    // "haikuctl stop --force critical"
    haikuctl_p.start("force-stop", {"-p", socket_path, "stop", "--force", "critical"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq(read_file_contents(igr_input_basedir + "/force-stop/expected-3"), haikuctl_p.get_stdout());
    igr_assert_eq("", haikuctl_p.get_stderr());

    // haiku should stop since all services are now stopped
    haiku_p.wait_for_term({1, 0});
}

void restart_test()
{
    // Restart both a process service and an internal service.

    igr_test_setup setup("restart");
    std::string output_file = setup.prep_output_file("basic-ran");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("restart", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    // "haikuctl start boot" - wait until "boot" has fully started:
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("restart", {"-p", socket_path, "start", "boot"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    // "basic" is a process service. It has started, but we need to give it a little time to
    // write its output:
    nanosleepx(0, 1000000000u / 10u);

    igr_assert_eq("ran\n", read_file_contents(output_file));
    if (unlink(output_file.c_str()) == -1) {
        throw std::system_error(errno, std::generic_category(), "unlink");
    }

    // "haikuctl restart basic"
    haikuctl_p.start("restart", {"-p", socket_path, "restart", "basic"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    nanosleepx(0, 1000000000u / 10u);

    igr_assert_eq("ran\n", read_file_contents(output_file));

    // "haikuctl restart internal"
    haikuctl_p.start("restart", {"-p", socket_path, "restart", "internal"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);
    igr_assert_eq("Service restarted.\n", haikuctl_p.get_stdout());
}

void check_basic_test()
{
    igr_test_setup setup("check-basic");

    auto check_result = run_haikucheck("check-basic", {"-d", "sd"});
    igr_assert_eq(read_file_contents(igr_input_basedir + "/check-basic/expected.txt"), check_result.first);
    igr_assert(check_result.second == 1, "haikucheck exit status == 1");
}

void check_cycle_test()
{
    igr_test_setup setup("check-cycle");

    auto check_result = run_haikucheck("check-cycle", {"-d", "sd"});
    igr_assert_eq(read_file_contents(igr_input_basedir + "/check-cycle/expected.txt"), check_result.first);
    igr_assert(check_result.second == 1, "haikucheck exit status == 1");
}

void check_cycle2_test()
{
    igr_test_setup setup("check-cycle2");

    auto check_result = run_haikucheck("check-cycle2", {"-d", "sd"});
    igr_assert_eq(read_file_contents(igr_input_basedir + "/check-cycle2/expected.txt"), check_result.first);
    igr_assert(check_result.second == 1, "haikucheck exit status == 1");
}

void check_lint_test()
{
    igr_test_setup setup("check-lint");

    auto check_result = run_haikucheck("check-lint", {"-d", "sd"});
    igr_assert_eq(read_file_contents(igr_input_basedir + "/check-lint/expected.txt"), check_result.first);
    igr_assert(check_result.second == 1, "haikucheck exit status == 1");
}

void check_consumer_of_test()
{
    igr_test_setup setup("check-consumer-of");

    auto check_result = run_haikucheck("check-consumer-of", {"-d", "sd"});
    igr_assert_eq(read_file_contents(igr_input_basedir +
            "/check-consumer-of/expected.txt"), check_result.first);
    igr_assert(check_result.second == 1, "haikucheck exit status == 1");
}

void reload1_test()
{
    igr_test_setup setup("reload1");
    std::string socket_path = setup.prep_socket_path();

    // This test requires reloading services after modifying a service description,
    // which for convenience we do by replacing the entire service directory (sd1 with sd2).
    // In order to do that we create a symlink to sd1 and use the link as the directory, then we retarget
    // the link at sd2.

    std::string sd_dir = setup.get_output_dir() + "/sd";
    unlink(sd_dir.c_str());
    if (symlink((igr_input_basedir + "/reload1/sd1").c_str(), sd_dir.c_str()) == -1) {
        throw std::system_error(errno, std::generic_category(), "symlink");
    }

    haiku_proc haiku_p;
    haiku_p.start("reload1", {"-u", "-d", sd_dir.c_str(), "-p", socket_path, "-q"}, true);

    // "haikuctl list" and check output
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("reload1", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq("", haikuctl_p.get_stderr());
    igr_assert_eq(read_file_contents(igr_input_basedir + "/reload1/initial.expected"),
            haikuctl_p.get_stdout());

    // Replace service directory with sd2
    unlink(sd_dir.c_str());
    if (symlink((igr_input_basedir + "/reload1/sd2").c_str(), sd_dir.c_str()) == -1) {
        throw std::system_error(errno, std::generic_category(), "symlink");
    }

    // reload should fail: c not started but is a dependency in the new service description
    haikuctl_p.start("reload1", {"--quiet", "-p", socket_path, "reload", "boot"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq(read_file_contents(igr_input_basedir + "/reload1/output2.expected"),
            haikuctl_p.get_stderr());
    igr_assert_eq("", haikuctl_p.get_stdout());

    // if we start c, should then be able to do the reload
    haikuctl_p.start("reload1", {"--quiet", "-p", socket_path, "start", "c"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);
    igr_assert_eq("", haikuctl_p.get_stderr());
    igr_assert_eq("", haikuctl_p.get_stdout());

    haikuctl_p.start("reload1", {"--quiet", "-p", socket_path, "reload", "boot"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);
    igr_assert_eq("", haikuctl_p.get_stderr());
    igr_assert_eq("", haikuctl_p.get_stdout());

    // list again and check output
    haikuctl_p.start("reload1", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq("", haikuctl_p.get_stderr());
    igr_assert_eq(read_file_contents(igr_input_basedir + "/reload1/output3.expected"),
            haikuctl_p.get_stdout());
}

void reload2_test()
{
    igr_test_setup setup("reload2");
    std::string socket_path = setup.prep_socket_path();

    // This test requires reloading services after modifying a service description,
    // which for convenience we do by replacing the entire service directory (sd1 with sd2).
    // In order to do that we create a symlink to sd1 and use the link as the directory, then we retarget
    // the link at sd2.

    std::string sd_dir = setup.get_output_dir() + "/sd";
    unlink(sd_dir.c_str());
    if (symlink((igr_input_basedir + "/reload2/sd1").c_str(), sd_dir.c_str()) == -1) {
        throw std::system_error(errno, std::generic_category(), "symlink");
    }

    haiku_proc haiku_p;
    haiku_p.start("reload2", {"-u", "-d", sd_dir.c_str(), "-p", socket_path, "-q"}, true);

    // Start "hold" service (allows us to stop "boot" without stopping haiku)
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("reload2", {"-p", socket_path, "start", "hold"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    // "haikuctl list" and check output
    haikuctl_p.start("reload2", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq("", haikuctl_p.get_stderr());
    igr_assert_eq(read_file_contents(igr_input_basedir + "/reload2/initial.expected"),
            haikuctl_p.get_stdout());

    // "haikuctl stop boot"
    haikuctl_p.start("reload2", {"-p", socket_path, "stop", "boot"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);

    // Replace service directory with sd2
    unlink(sd_dir.c_str());
    if (symlink((igr_input_basedir + "/reload2/sd2").c_str(), sd_dir.c_str()) == -1) {
        throw std::system_error(errno, std::generic_category(), "symlink");
    }

    // "haikuctl reload boot", should succeed
    haikuctl_p.start("reload2", {"-p", socket_path, "reload", "boot"});
    int haikuctl_result = haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);
    igr_assert(haikuctl_result == 0, "\"haikuctl reload boot\" returned unexpected status");
    igr_assert_eq(read_file_contents(igr_input_basedir + "/reload2/output2.expected"),
            haikuctl_p.get_stdout());
    igr_assert_eq("", haikuctl_p.get_stderr());

    // "haikuctl start boot"
    haikuctl_p.start("reload2", {"-p", socket_path, "start", "boot"});
    haikuctl_result = haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);
    igr_assert(haikuctl_result == 0, "\"haikuctl start boot\" returned unexpected status");

    // "haikuctl list"
    haikuctl_p.start("reload2", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}  /* max 1 second */);
    igr_assert_eq(read_file_contents(igr_input_basedir + "/reload2/output3.expected"),
            haikuctl_p.get_stdout());
    igr_assert_eq("", haikuctl_p.get_stderr());
}

void no_command_error_test()
{
    // Check that a service without a command configured causes the appropriate error.

    igr_test_setup setup("no-command-error");
    std::string socket_path = setup.prep_socket_path();

    std::string sd_dir = setup.get_output_dir() + "/sd";

    std::vector<std::string> haiku_args {"-u", "-d", "sd", "-p", socket_path};

#if SUPPORT_CGROUPS
    // If cgroups support, supply dummy cgroup base path to avoid "unable to determine cgroup" message
    haiku_args.push_back("-b");
    haiku_args.push_back("/");
#endif

    haiku_args.push_back("no-command");

    haiku_proc haiku_p;
    haiku_p.start("no-command-error", haiku_args);
    haiku_p.wait_for_term({1, 0}  /* max 1 second */);

    igr_assert_eq(read_file_contents(igr_input_basedir + "/no-command-error/haiku-run.expected"), haiku_p.get_stdout());
}

void add_rm_dep_test()
{
    // Tests for adding/removing dependencies at run time
    igr_test_setup setup("add-rm-dep");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("add-rm-dep", {"-u", "-d", "sd", "-p", socket_path, "-q", "main"}, true);

    // "main" and "secondary" should both be running
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected1"), haikuctl_p.get_stdout());

    // remove dependency from main to secondary
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "rm-dep", "waits-for", "main", "secondary"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected2"), haikuctl_p.get_stdout());

    // "secondary" should stop as a result
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected3"), haikuctl_p.get_stdout());

    // re-add the dependency
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "add-dep", "waits-for", "main", "secondary"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected4"), haikuctl_p.get_stdout());

    // re-adding won't affect "secondary", it remains stopped (soft dependency)
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected3"), haikuctl_p.get_stdout());

    // It should be possible to "wake" the "secondary" service since it has a soft dependent
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "wake", "secondary"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected5"), haikuctl_p.get_stdout());

    // Check final state of services (same as original state)
    haikuctl_p.start("add-rm-dep", {"-p", socket_path, "list"});
    haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert_eq(read_file_contents(igr_input_basedir + "/add-rm-dep/expected1"), haikuctl_p.get_stdout());
}

void var_subst_test()
{
    // Tests for variable substitution
    igr_test_setup setup("var-subst");
    std::string socket_path = setup.prep_socket_path();

    setup.prep_output_file("args-record");

    igr_env_var_setup env_test_var_one("TEST_VAR_ONE", "var one");
    igr_env_var_setup env_test_var_two("TEST_VAR_TWO", "vartwo");
    igr_env_var_setup env_test_var_three("TEST_VAR_THREE", "varthree");

    haiku_proc haiku_p;
    haiku_p.start("var-subst", {"-u", "-d", "sd", "-p", socket_path, "-q", "checkargs"}, false);
    int status = haiku_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haiku did not exit cleanly");

    igr_assert_eq("1:xxxvar one/yyy 2:vartwovarthree 3:varfour 4:\n",
            read_file_contents(setup.get_output_dir() + "/args-record"));
}

void svc_start_fail_test()
{
    // Tests for service start failure
    igr_test_setup setup("svc-start-fail");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("svc-start-fail", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    haikuctl_proc haikuctl_p;
    haikuctl_p.start("svc-start-fail",{"-u", "-p", socket_path, "start", "bad-command"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */

    igr_assert(status == 1, "haikuctl did not return error code");
    igr_assert_eq(read_file_contents(igr_input_basedir + "/svc-start-fail/expected-1"),
            haikuctl_p.get_stdout());

    haikuctl_p.start("svc-start-fail",{"-u", "-p", socket_path, "start", "timeout-command"});
    status = haikuctl_p.wait_for_term({2, 0}); /* max 2 seconds */

    igr_assert(status == 1, "haikuctl did not return error code");
    igr_assert_eq(read_file_contents(igr_input_basedir + "/svc-start-fail/expected-2"),
            haikuctl_p.get_stdout());
    igr_assert_eq("", haikuctl_p.get_stderr());
}

void dep_not_found_test()
{
    igr_test_setup setup("dep-not-found");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("dep-not-found", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    haikuctl_proc haikuctl_p;
    haikuctl_p.start("svc-start-fail", {"-u", "-p", socket_path, "start", "missing-dep-svc"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */

    igr_assert(status == 1, "haikuctl did not return error code");
    igr_assert_eq(read_file_contents(igr_input_basedir + "/dep-not-found/output.expected"),
            haikuctl_p.get_stderr());
    igr_assert_eq("", haikuctl_p.get_stdout());
}

void pseudo_cycle_test()
{
    igr_test_setup setup("pseudo-cycle");

    // This test has three services, which have no cycle but do have a "before" relationship:
    //
    // boot
    //    depends-on -->
    // middle
    //    depends-on -->
    // service
    //    before = boot
    //
    // This should not be considered a cyclic dependency. The service script should run.

    std::string output_file = setup.prep_output_file("svc-script");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("pseudo-cycle", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);
    int status = haiku_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haiku did not exit cleanly");

    igr_assert_eq("ran\n", read_file_contents(output_file));
}

void before_after_test()
{
    // Tests around before/after link functionality
    igr_test_setup setup("before-after");

    std::string script_output_file = setup.prep_output_file("script-output");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("before-after", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    // start parent; should start service2 and then service1 (due to before= in service2).
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "start", "parent"});
    int status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    // Note service1 takes longer to start, but has a "before" service2 so should still start first.
    // service3 is similarly "after" service2.
    igr_assert_eq("one\n" "two\n" "three\n", read_file_contents(script_output_file));

    if (unlink(script_output_file.c_str()) != 0) {
        throw std::system_error(errno, std::generic_category(), std::string("unlink: ") + script_output_file);
    }

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "stop", "parent"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "unload", "parent"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "unload", "service2"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "reload", "service2"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "start", "parent"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    igr_assert_eq("one\n" "two\n" "three\n", read_file_contents(script_output_file));

    if (unlink(script_output_file.c_str()) != 0) {
        throw std::system_error(errno, std::generic_category(), std::string("unlink: ") + script_output_file);
    }

    haiku_p.signal(SIGTERM);
    status = haiku_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haiku did not exit cleanly");

    haiku_p.start("before-after", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    // load without loading parent: force service2 loaded first
    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "reload", "service2"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "reload", "service1"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "start", "--no-wait", "service1"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after", {"-u", "-p", socket_path, "start", "service2"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    igr_assert_eq("one\n" "two\n", read_file_contents(script_output_file));
}

void before_after2_test()
{
    // Tests around before/after link functionality
    igr_test_setup setup("before-after2");
    std::string socket_path = setup.prep_socket_path();

    std::string script_output_file = setup.prep_output_file("script-output");

    haiku_proc haiku_p;
    haiku_p.start("before-after2", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    // service2 depends on service1, and service1 is "before" service2
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("before-after2", {"-u", "-p", socket_path, "reload", "service2"});
    int status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    // Remove the depends-on dependency from service2 to service1
    haikuctl_p.start("before-after2", {"-u", "-p", socket_path, "rm-dep", "need", "service2", "service1"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    // Start both service1 and service2; service1 takes longer to start, but the "before" should prevent
    // service2 from starting until service2 has started
    haikuctl_p.start("before-after2", {"-u", "-p", socket_path, "start", "--no-wait", "service1"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("before-after2", {"-u", "-p", socket_path, "start", "service2"});
    status = haikuctl_p.wait_for_term({5, 0}); /* max 5 seconds */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    igr_assert_eq("one\n" "two\n", read_file_contents(script_output_file));
}

void log_via_pipe_test()
{
    igr_test_setup setup("log-via-pipe");

    std::string logged_output_file = setup.prep_output_file("logged-output");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("log-via-pipe", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    nanosleepx(0, 1000000000u / 10u);

    igr_assert_eq(read_file_contents(logged_output_file), "");

    haikuctl_proc haikuctl_p;
    haikuctl_p.start("log-via-pipe", {"-u", "-p", socket_path, "start", "producer"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("log-via-pipe", {"-u", "-p", socket_path, "stop", "producer"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    nanosleepx(0, (1000000000u / 10u) * 2u);

    igr_assert_eq(read_file_contents(logged_output_file), "Producing output...\n");

    haikuctl_p.start("log-via-pipe", {"-u", "-p", socket_path, "start", "producer"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("log-via-pipe", {"-u", "-p", socket_path, "stop", "producer"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    nanosleepx(0, (1000000000u / 10u) * 2u);

    igr_assert_eq(read_file_contents(logged_output_file), "Producing output...\n" "Producing output...\n");
}

void catlog_test()
{
    igr_test_setup setup("catlog");
    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("catlog", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    // wait until "output" has actually started
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("catlog", {"-u", "-p", socket_path, "start", "output"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("catlog", {"-u", "-p", socket_path, "catlog", "output"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    igr_assert_eq("Output...\n", haikuctl_p.get_stdout());

    // Check output again, this time also clear the buffer

    haikuctl_p.start("catlog", {"-u", "-p", socket_path, "catlog", "--clear", "output"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    igr_assert_eq("Output...\n", haikuctl_p.get_stdout());

    // Check a third time, buffer should be clear now

    haikuctl_p.start("catlog", {"-u", "-p", socket_path, "catlog", "--clear", "output"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    igr_assert_eq("", haikuctl_p.get_stdout());
}

void offline_enable_test()
{
    igr_test_setup setup("offline-enable");

    std::string sd_dir = setup.get_output_dir() + "/sd";

    if (faccessat(AT_FDCWD, sd_dir.c_str(), F_OK, AT_EACCESS) == -1) {
        if (errno != ENOENT) {
            throw std::system_error(errno, std::generic_category(), std::string("faccessat: ") + sd_dir);
        }
    }
    else {
        rm_r(sd_dir.c_str());
    }

    mkdir(sd_dir.c_str(), S_IRWXU);
    cp_file((igr_input_basedir + "/offline-enable/sd/A").c_str(), (sd_dir + "/A").c_str());
    cp_file((igr_input_basedir + "/offline-enable/sd/boot").c_str(), (sd_dir + "/boot").c_str());
    mkdir((sd_dir + "/boot.d").c_str(), S_IRWXU);

    // run_haikuctl $QUIET --offline -d "$IGR_OUTPUT/sd" enable A
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("offline-enable", {"-u", "--offline", "-d", sd_dir.c_str(), "enable", "A"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    if (faccessat(AT_FDCWD, (sd_dir + "/boot.d/A").c_str(), F_OK, AT_EACCESS) == -1) {
        throw std::system_error(errno, std::generic_category(), std::string("faccessat: ") + sd_dir);
    }

    haikuctl_p.start("offline-enable", {"-u", "--offline", "-d", sd_dir.c_str(), "disable", "A"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    bool file_gone = false;
    if (faccessat(AT_FDCWD, (sd_dir + "/boot.d/A").c_str(), F_OK, AT_EACCESS) == -1) {
        if (errno == ENOENT) {
            file_gone = true;
        }
        else {
            throw std::system_error(errno, std::generic_category(), std::string("faccessat: ") + sd_dir);
        }
    }

    igr_assert(file_gone, "Service A not disabled after disable command; sd/boot.d/A still exists");
    rm_tree(sd_dir.c_str());
}

void xdg_config_test()
{
    igr_test_setup setup("xdg-config");

    std::string ran_marker_file = setup.prep_output_file("basic-ran");
    std::string socket_path = setup.prep_socket_path();

    std::string config_dir = igr_input_basedir + "/xdg-config/config";

    igr_env_var_setup env_output("XDG_CONFIG_HOME", config_dir.c_str());

    haiku_proc haiku_p;
    haiku_p.start("xdg-config", {"-u", "-p", socket_path, "-q", "basic"}, false);
    haiku_p.wait_for_term({1, 0}); /* max 1 second */

    igr_assert_eq("ran\n", read_file_contents(ran_marker_file));
}

void cycles_test()
{
    igr_test_setup setup("cycles");

    std::string socket_path = setup.prep_socket_path();

    haiku_proc haiku_p;
    haiku_p.start("cycles", {"-u", "-p", socket_path, "-d", "sd", "-q"}, true);

    // "after"-cycle:
    //  ac depends-on ac1, ac2
    //  ac1 is "after" ac2
    //  ac2 is "after" ac1

    haikuctl_proc haikuctl_p;
    haikuctl_p.start("cycles", {"-u", "-p", socket_path, "start", "ac"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status != 0, "haikuctl unexpectedly exited cleanly");

    igr_assert_eq(read_file_contents(igr_input_basedir + "/cycles/expected-ac"), haikuctl_p.get_stderr());

    // before-after conflict:
    //  ba depends on ba1, ba2
    //  ba2 is both before and after ba1

    haikuctl_p.start("cycles", {"-u", "-p", socket_path, "start", "ba"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status != 0, "haikuctl unexpectedly exited cleanly");

    igr_assert_eq(read_file_contents(igr_input_basedir + "/cycles/expected-ba"), haikuctl_p.get_stderr());

    // "before_self" is before itself

    haikuctl_p.start("cycles", {"-u", "-p", socket_path, "start", "before_self"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status != 0, "haikuctl unexpectedly exited cleanly");

    igr_assert_eq(read_file_contents(igr_input_basedir + "/cycles/expected-before_self"),
            haikuctl_p.get_stderr());

    // "after_self" is after itself

    haikuctl_p.start("cycles", {"-u", "-p", socket_path, "start", "after_self"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status != 0, "haikuctl unexpectedly exited cleanly");

    igr_assert_eq(read_file_contents(igr_input_basedir + "/cycles/expected-after_self"),
            haikuctl_p.get_stderr());
}

void svc_arg_test()
{
    igr_test_setup setup("svc-arg");
    std::string output_file = setup.prep_output_file("svc-arg-record");
    std::string socket_path = setup.prep_socket_path();

    igr_env_var_setup env_output("OUTPUT", output_file.c_str());

    haiku_proc haiku_p;
    haiku_p.start("svc-arg", {"-u", "-d", "sd", "-p", socket_path, "-q", "checkarg@foo"});
    haiku_p.wait_for_term({1,0} /* max 1 second */);

    check_file_contents(output_file, std::string() +
            "test-$1\n" +
            "test-hello\n" +
            "hello\n" +
            "foo\n" +
            "foo\n" +
            "bar\n");
}

void svc_arg_consumer_test()
{
    // Test for using minimal variable substitution in consumer services
    igr_test_setup setup("svc-arg-consumer");

    std::string logged_output_file = setup.prep_output_file("logged-output");
    std::string socket_path = setup.prep_socket_path();

    // "boot" service brings up "consumer" service and consumer writes its $1 value
    haiku_proc haiku_p;
    haiku_p.start("svc-arg-consumer", {"-u", "-d", "sd", "-p", socket_path, "-q"}, true);

    nanosleepx(0, 1000000000u / 10u);

    igr_assert_eq(read_file_contents(logged_output_file), "producer\n");

    // Start and stop the producer for printing output to consumer
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("svc-arg-consumer", {"-u", "-p", socket_path, "start", "producer"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("svc-arg-consumer", {"-u", "-p", socket_path, "stop", "producer"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    nanosleepx(0, (1000000000u / 10u) * 2u);

    igr_assert_eq(read_file_contents(logged_output_file), "producer\n" "Producing output...\n");

    // Another start and stop to make sure that producer is linked to consumer properly
    haikuctl_p.start("svc-arg-consumer", {"-u", "-p", socket_path, "start", "producer"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    haikuctl_p.start("svc-arg-consumer", {"-u", "-p", socket_path, "stop", "producer"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    nanosleepx(0, (1000000000u / 10u) * 2u);

    igr_assert_eq(read_file_contents(logged_output_file), "producer\n" "Producing output...\n" "Producing output...\n");
}

void svc_arg_enable_test()
{
    igr_test_setup setup("svc-arg-enable");
    std::string socket_path = setup.prep_socket_path();

    std::string sd_dir = setup.get_output_dir() + "/sd";

    if (faccessat(AT_FDCWD, sd_dir.c_str(), F_OK, AT_EACCESS) == -1) {
        if (errno != ENOENT) {
            throw std::system_error(errno, std::generic_category(), std::string("faccessat: ") + sd_dir);
        }
    }
    else {
        rm_r(sd_dir.c_str());
    }

    mkdir(sd_dir.c_str(), S_IRWXU);
    cp_file((igr_input_basedir + "/svc-arg-enable/sd/A").c_str(), (sd_dir + "/A").c_str());
    cp_file((igr_input_basedir + "/svc-arg-enable/sd/boot").c_str(), (sd_dir + "/boot").c_str());
    mkdir((sd_dir + "/boot.d").c_str(), S_IRWXU);

    // haiku
    haiku_proc haiku_p;
    haiku_p.start("svc-arg-enable", {"-u", "-d", sd_dir.c_str(), "-p", socket_path, "-q"}, true);

    // haikuctl -d "$IGR_OUTPUT/sd" enable A@1
    haikuctl_proc haikuctl_p;
    haikuctl_p.start("svc-arg-enable", {"-p", socket_path.c_str(), "-d", sd_dir.c_str(), "enable", "A@1"});
    int status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    if (faccessat(AT_FDCWD, (sd_dir + "/boot.d/A@1").c_str(), F_OK, AT_EACCESS) == -1) {
        throw std::system_error(errno, std::generic_category(), std::string("faccessat: ")
                + sd_dir + "/boot.d/A@1");
    }

    haikuctl_p.start("svc-arg-enable", {"-p", socket_path.c_str(), "-d", sd_dir.c_str(), "disable", "A@1"});
    status = haikuctl_p.wait_for_term({1, 0}); /* max 1 second */
    igr_assert(status == 0, "haikuctl did not exit cleanly");

    bool file_gone = false;
    if (faccessat(AT_FDCWD, (sd_dir + "/boot.d/A@1").c_str(), F_OK, AT_EACCESS) == -1) {
        if (errno == ENOENT) {
            file_gone = true;
        }
        else {
            throw std::system_error(errno, std::generic_category(), std::string("faccessat: ") + sd_dir);
        }
    }

    igr_assert(file_gone, "Service A@1 not disabled after disable command; sd/boot.d/A@1 still exists");
    rm_tree(sd_dir.c_str());
}
