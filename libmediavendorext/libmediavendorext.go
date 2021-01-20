
package libmediavendorext

import (
	"strconv"

	"android/soong/android"
	"android/soong/cc"

	"github.com/google/blueprint/proptools"
)

func init() {
	android.RegisterModuleType("mediaext_defaults", mediaextDefaultsFactory)
}

func mediaextDefaultsFactory() android.Module {
	module := cc.DefaultsFactory()
	android.AddLoadHook(module, mediaextDefaults)

	return module
}

func mediaextDefaults(ctx android.LoadHookContext) {
	type props struct {
		System_ext_specific *bool
	}
	p := &props{}

	// for version bigger 30, add System_ext_specific := true
	// before the Android version is released code name is not REL
	// and PLATFORM_VNDK_VERSION is code name
	if ctx.Config().PlatformSdkCodename() == "REL" {
		ver, _ := strconv.Atoi(ctx.DeviceConfig().PlatformVndkVersion())
		if ver < 30 {
			return
		}
	}

	p.System_ext_specific = proptools.BoolPtr(true)
	ctx.AppendProperties(p)
}