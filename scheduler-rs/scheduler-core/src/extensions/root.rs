use abi_stable::library::RootModule;
use abi_stable::sabi_types::version::VersionStrings;
use abi_stable::std_types::RBox;

use super::api::SchedulerExtension_TO;

#[repr(C)]
#[derive(abi_stable::StableAbi)]
#[sabi(kind(Prefix(prefix_ref = ExtensionRootModuleRef, prefix_fields = ExtensionRootModule_Prefix)))]
pub struct ExtensionRootModule {
    #[sabi(last_prefix_field)]
    pub get_extension: extern "C" fn() -> SchedulerExtension_TO<'static, RBox<()>>,
}

impl RootModule for ExtensionRootModuleRef {
    abi_stable::declare_root_module_statics! {ExtensionRootModuleRef}
    const BASE_NAME: &'static str = "sched_extension";
    const NAME: &'static str = "sched_extension";
    const VERSION_STRINGS: VersionStrings = abi_stable::package_version_strings!();
}
