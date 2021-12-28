
package libmediavendorext

import (
	"android/soong/android"
	"android/soong/cc"
)

func init() {
	android.RegisterModuleType("mediaext_sdkversion", mediaextSdkVersionFactory)
}

func mediaextSdkVersionFactory() android.Module {
	module := cc.DefaultsFactory()
	android.AddLoadHook(module, mediaextSdkVersion)

	return module
}

func mediaextSdkVersion(ctx android.LoadHookContext) {
	type props struct {
		Cflags []string
	}
	p := &props{}

	p.Cflags = append(p.Cflags, "-DANDROID_PLATFORM_SDK_VERSION=" + ctx.AConfig().PlatformSdkVersion())
	ctx.AppendProperties(p)
}
