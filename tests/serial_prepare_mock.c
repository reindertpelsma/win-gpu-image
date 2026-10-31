/* Original CPU doubles for the actual guarded serial policy. */
#define WGI_SERIAL_TEST
#include "../programs/serial-prepare.c"
static const char *scenario;
static int calls, selected, checks;
static WGI_SERIAL_SNAPSHOT baseline;
static int select_current(void *ctx, const WGI_SERIAL_SNAPSHOT *s) {
    (void)ctx; (void)s; ++selected;
    return strcmp(scenario, "select_failed") && strcmp(scenario, "wrong_selected");
}
static int call_install(void *ctx, unsigned step) {
    (void)ctx; ++calls;
    printf("MOCK_CALL step=%u\n", step);
    return !(strstr(scenario, "call_fail") && step == 1);
}
static int inspect(void *ctx, WGI_SERIAL_SNAPSHOT *s, unsigned *flags) {
    (void)ctx; ++checks; *s = baseline; *flags = 0;
    if (!strcmp(scenario, "check_failed")) return 0;
    if (!strcmp(scenario, "identity_changed")) strcpy(s->provider, "Other");
    if (!strcmp(scenario, "reboot_first") && checks == 2) *flags = WGI_REBOOT;
    if (!strcmp(scenario, "restart_second") && checks == 3) *flags = WGI_RESTART;
    if (!strcmp(scenario, "preexisting_reboot") && checks == 1) *flags = WGI_REBOOT;
    return 1;
}
static void fill(void) {
    memset(&baseline, 0, sizeof(baseline));
    baseline.count = baseline.started = baseline.installer_ok = 1;
    strcpy(baseline.id, "ACPI\\PNP0501\\1");
    strcpy(baseline.class_name, "{4d36e978-e325-11ce-bfc1-08002be10318}");
    strcpy(baseline.service, "Serial"); strcpy(baseline.inf, "msports.inf");
    strcpy(baseline.provider, "Microsoft"); strcpy(baseline.section, "ComPort.NT");
    strcpy(baseline.matching, "acpi\\pnp0501");
}
int main(int argc, char **argv) {
    if (argc != 2) return 3;
    scenario = argv[1]; fill();
    if (!strcmp(scenario, "duplicate")) baseline.count = 2;
    if (!strcmp(scenario, "parallel")) strcpy(baseline.id, "ACPI\\PNP0400\\1");
    if (!strcmp(scenario, "wrong_class")) strcpy(baseline.class_name, "other");
    if (!strcmp(scenario, "wrong_service")) strcpy(baseline.service, "Other");
    if (!strcmp(scenario, "not_started")) baseline.started = 0;
    if (!strcmp(scenario, "problem")) baseline.problem = 28;
    if (!strcmp(scenario, "wrong_inf")) strcpy(baseline.inf, "other.inf");
    if (!strcmp(scenario, "wrong_provider")) strcpy(baseline.provider, "Other");
    if (!strcmp(scenario, "missing_section")) baseline.section[0] = 0;
    if (!strcmp(scenario, "wrong_matching")) strcpy(baseline.matching, "acpi\\pnp0400");
    if (!strcmp(scenario, "installer_missing")) baseline.installer_ok = 0;
    if (!strcmp(scenario, "name_present")) baseline.name_state = 1;
    if (!strcmp(scenario, "name_unknown")) baseline.name_state = -1;
    if (!strcmp(scenario, "com_unknown")) baseline.com_state = -1;
    if (!strcmp(scenario, "com_present")) baseline.com_state = 1;
    if (!strcmp(scenario, "typed_bad") || !strcmp(scenario, "typed_bounds") ||
        !strcmp(scenario, "typed_nul") || !strcmp(scenario, "typed_embedded")) {
        char data[8] = {'a', 'b', 'c', 0};
        unsigned type = 1, bytes = 4;
        if (!strcmp(scenario, "typed_bad")) type = 7;
        if (!strcmp(scenario, "typed_bounds")) { data[3] = 'x'; data[4] = 0; bytes = 5; }
        if (!strcmp(scenario, "typed_nul")) data[3] = 'x';
        if (!strcmp(scenario, "typed_embedded")) data[1] = 0;
        int valid = wgi_serial_string(data, type, bytes, 4);
        printf("MOCK_TYPED valid=%d\n", valid); return valid ? 2 : 0;
    }
    WGI_SERIAL_OPS ops = {select_current, call_install, inspect};
    int rc = wgi_serial_run(&baseline, &ops, NULL);
    printf("MOCK_END rc=%d calls=%d selected=%d checks=%d\n", rc, calls, selected, checks);
    return rc;
}
