# Example 0 - Basics

`tadah` is a command-line tool designed to simplify the process of training and evaluating machine learning models. It provides a user-friendly interface for managing configurations, datasets, and training processes. It is the main entry point to other subcommands, which are used to perform specific tasks related to machine learning workflows.

In this example, we demonstrate how to access help information using `tadah` binary and how to use the `explain` subcommand to understand the purpose of different subcommands and their options.

Ensure Tadah! is installed and accessible through your `$PATH`. If not, use the full path to the Tadah! binary in your commands.


## Accessing Tadah! Help

`tadah` binary provides a help command to display information about the available subcommands and their options. This is useful for users who are new to the tool or need a refresher on its capabilities.

To display a list of subcommands and their short descriptions, use:

```bash
tadah --help
```

or

```bash
tadah -h
```

## Subcommand Help

For help with specific subcommands, such as `train`, use:

```bash
tadah train --help
```

## Tadah! explain command

Tadah! provides simple `explain` subcommand to help you understand the purpose of each tool and its options. The `explain` subcommand takes single argument. The argument represents query about either particular (sub)command or its option. It will print an extended description for the command or option, including its purpose, usage, and any additional information that may be helpful.

For example, to learn about the `train` `--task` option, use:

```bash
tadah explain train.task
```

or for a second level subcommand, such as `tadah data split`, use:

```bash
tadah explain data.balance
```

and for say for `--dbfile` option of `tadah data split`, use:

```bash
tadah explain data.split.dbfile
```

## Runtime Errors

Tadah! provides useful runtime errors, such as when a configuration file is missing a necessary key. While we strive for reliability, no code is entirely bug-free. If you encounter issues, please report them on Tadah!'s GitLab repository.
