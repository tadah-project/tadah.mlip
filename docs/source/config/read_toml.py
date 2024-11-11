'''
This script generates documentation in rst format from
the TOML dictionary.
'''
import pytomlpp as p
import os
import shutil
import subprocess

def download_toml_file(repo_url, file_path, output_path):
    try:
        # Clone the repository into a temporary directory
        repo_name = "temp_repo"
        subprocess.run(['git', 'clone', '--depth', '1', repo_url, repo_name], check=True)

        # Copy the desired file to the output path
        full_file_path = os.path.join(repo_name, file_path)
        if os.path.exists(full_file_path):
            shutil.copy(full_file_path, output_path)
            print(f"Successfully downloaded {file_path} to {output_path}")
        else:
            print(f"File {file_path} not found in the repository.")

    except subprocess.CalledProcessError as e:
        print(f"Error handling the repository: {e}")
    finally:
        # Clean up
        if os.path.exists(repo_name):
            shutil.rmtree(repo_name)

download_toml_file(
    'https://git.ecdf.ed.ac.uk/tadah/core.git',
    'config/config_keys.toml',
    '../docs/source/config/config_keys.toml'
)

# Path to TOML dict
# d = p.load("../src/configs/config_keys.toml",'r')
d = p.load("../docs/source/config/config_keys.toml",'r')

# Path to rst outfile
# f = open("source/config/config_keys.rst", "w")
f = open("../docs/source/config/config_keys.rst", "w")

header='''
Config KEYS
===========

This section describes the format of the configurational file as used by Tadah!.

The primary building block of a configurational file is a KEY/VALUE pair.
Each KEY/VALUE pair must be on the same line - no more than one key on the same line.
The KEY is always a <string> type and must appear first on the line
followed by the VALUE. The format and type of a VALUE is specific to a given KEY.

It is usually the case that only a small subset of KEYs is being used to train
the model. Tadah! will throw an error if required key is missing:

.. code-block:: bash

    [user@host:~] $ tadah train -c config.train
    terminate called after throwing an instance of 'std::runtime_error'
      what():  Key not found: DBFILE
    Aborted (core dumped)

Above code indicates that the DBFILE_ KEY has not been specified by the user in
the :file:`config.train` file. Adding the DBFILE key and the corresponding
value to the :file:`config.train` will solve the issue.

Note that the meaning for some of the KEYs may vary depending on what model
or descriptor is being used.
The documentation of each model or descriptor lists what KEYs are required
and provides some further explanation.

The '#' symbol can be used for comments.

The `Max number of values:` line indicates the maximum values which
can be appended to the KEY. It can be done by either:

.. code-block:: bash

    KEY VALUE1 VALUE2 VALUE3

or by repeating the KEY

.. code-block:: bash

    KEY VALUE1
    KEY VALUE2
    KEY VALUE3

If the number of values exceed maximum allowed the code will throw an error.
For example, defining RCTYPE2B_ twice where only one value is allowed results
in the following error message:

.. code-block:: bash

    [user@host:~] $ tadah train -c config.train
    terminate called after throwing an instance of 'std::runtime_error'
      what():  Repeated key RCTYPE2B Cut_Cos
    Aborted (core dumped)
\n
'''
f.write(header)

for key,val in d.items():
    f.write(".. _%s:\n\n" % key)    # create reference
    f.write("%s\n" % key)
    f.write("%s\n" % (len(key)*"."))
    if val['value_format']!='' and val['value_type']!='':
        f.write("%s" % (key))
        for vtype,vfmt in zip(val['value_type'],val['value_format']):
            f.write(" [*%s*] %s" % (vtype,vfmt))
        f.write("\n")
    if len(val['value_N'])!=0:
        f.write("   *Max number of values*:")
        for valN in val['value_N']:
            f.write("  %s" % abs(valN))
        f.write("\n\n")
    if len(val['default'])!=0:
        f.write("   *Default*:")
        for valN in val['default']:
            f.write("  %s" % (str(valN).lower()
                              if type(valN) is bool else valN))
        f.write("\n\n")
    if len(val['description'])!=0:
        f.write("   *Description*:\n\n")
        f.write("   %s\n\n" % val['description'])
    if len(val['examples'])!=0:
        for i,example in enumerate(val['examples']):
            f.write("   *Example*: %d ::\n\n" % (i+1))
            f.write("       %s\n\n" % example)

    # Deal with suboptions
    for key2,val2 in val.items():
        if type(val2) is dict:
            if val2['value_format']!='' and val2['value_type']!='':
                f.write("%s" % (key))
                for vtype,vfmt in zip(val2['value_type'],val2['value_format']):
                    f.write(" [*%s*] %s" % (vtype,vfmt))
                f.write("\n")
            if len(val2['value_N'])!=0:
                f.write("   *Max number of values*: ")
                for valN in val2['value_N']:
                    f.write("  %s" % abs(valN))
                f.write("\n\n")
            if len(val2['description'])!=0:
                f.write("   *Description*:\n\n")
                f.write("   %s\n\n" % val2['description'])
            if len(val2['examples'])!=0:
                for i,example in enumerate(val2['examples']):
                    f.write("   *Example*: %d ::\n\n" % (i+1))
                    f.write("       %s\n\n" % example)

f.close()
