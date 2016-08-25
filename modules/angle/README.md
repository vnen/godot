# ANGLE Third-Party library

Main website: http://angleproject.org

Extracted from ANGLE project [master branch](https://chromium.googlesource.com/angle/angle)
at [commit 5655b849d8fe1e79d06f2ed0b5064533ab893a44](https://chromium.googlesource.com/angle/angle/+/5655b849d8fe1e79d06f2ed0b5064533ab893a44).

## Copy and update instructions

* Follow the ANGLE instructions to [setup the dev environment](https://chromium.googlesource.com/angle/angle/+/master/doc/DevSetup.md).
This is needed to fetch the third-party ANGLE dependencies.

* Copy all the folders inside ANGLE `src` folder (except for `test`) to the `modules/angle/thirdparty` folder of Godot source.

* Copy the informational files from ANGLE (like `README`, `AUTHORS`, `LICENSE`, etc.) to the `modules/angle/thirdparty` folder of Godot source.

* Get a list of all files and split it into the specifific variables at `modules/angle/SCsub`. You can either analyze the path to decide (`d3d`, `gl`, etc.)
or look at the `.gypi` files.

* There might be a need to update flags. If it does not build at first, check the `.gypi` files and/or Visual Studio projects for preprocessor definitions.

* Update this file with the new commit.

## Special notes

Some of the flags needs to be applied to the **whole build** and not only to the ANGLE files (such as `GL_APICALL` and `EGLAPI`).
Those need to be added to the main SCons `env` and not to the cloned `env_angle` environment.

Currently this is enabled only for the WinRT platform. Other platforms must be enabled on `config.py` and also properly configured on `SCsub`.
