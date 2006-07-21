sbin-$(CONFIG_INIT_WRAPPER) +=		init.wrapper

install-local-$(CONFIG_INIT_WRAPPER):	install-local-setup-init-wrapper


install-local-setup-init-wrapper:	$(DEPS_INSTALL)/init-wrapper

$(DEPS_INSTALL)/init-wrapper:		$(FSSETUP_DIR)/setup.mk
		${QS} 'LOCAL' 'init-wrapper'
		-rm -f $@
		rm -f $(TARGET_DIR)/sbin/init
		ln -s init.wrapper $(TARGET_DIR)/sbin/init
		${Q}touch $@
