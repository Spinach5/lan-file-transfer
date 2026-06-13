/* Minimal main for CLI-only build (no SDL2 dependency) */
int cli_main(int argc, char **argv);

int main(int argc, char **argv)
{
    return cli_main(argc, argv);
}
