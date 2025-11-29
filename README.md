# NeoShell - The Terminal That Speaks Human

Welcome! NeoShell is a friendly terminal that understands natural language commands. No more memorizing cryptic Unix commands - just type what you mean.

## What Makes It Special?

Instead of typing `ls -la`, just type `list` or `show`.  
Instead of `pwd`, type `whereami`.  
Instead of `mkdir`, type `makedir`.

It's that simple.

## Quick Start

### Compile It

```bash
g++ -o neoshell.exe neoshell.cpp -std=c++11
```

### Run It

```bash
neoshell.exe
```

That's it! You're ready to go.

## Try These Commands

```bash
list                    # Show files in current folder
whereami                # Where am I right now?
goto Documents          # Go to Documents folder
makedir projects        # Create a new folder
read myfile.txt         # Read a file
print Hello World       # Display some text
who                     # Show your username
when                    # What time is it?
```

## Smart Suggestions

Made a typo? NeoShell will help:

```bash
> lst
Did you mean:
  list
  last
```

## Useful Features

**Save Your Favorite Places**

```bash
bookmark add work       # Save current location
bookmark go work        # Jump back instantly
```

**Create Shortcuts**

```bash
alias ll=list           # Create your own commands
```

**Quick Calculator**

```bash
calc 15*20              # Do quick math
```

**Todo List**

```bash
todo add Fix the bug
todo list
todo done 1
```

**Quick Notes**

```bash
note Remember to deploy tomorrow
```

## Get Help Anytime

```bash
help                    # See all available commands
```

## Three Themes

```bash
theme default           # Professional look
theme minimal           # Clean and simple
theme cyber             # Futuristic style
```

## Exit

```bash
exit                    # or quit, or bye
```

## That's All!

NeoShell is designed to be intuitive. If you think a command should work a certain way, it probably does. Just try it!

Made with care for humans who prefer speaking naturally over memorizing commands.
