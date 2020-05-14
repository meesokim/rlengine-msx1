export LOCAL_BUILD_SRC = $(CURDIR)
export LOCAL_BUILD_OUT = $(CURDIR)/out
export LOCAL_BUILD_RES = $(CURDIR)/res
export LOCAL_BUILD_RES_SPR = $(LOCAL_BUILD_RES)/spr
export LOCAL_BUILD_RES_MAP = $(LOCAL_BUILD_RES)/map
export LOCAL_BUILD_RES_TIL = $(LOCAL_BUILD_RES)/tile
export LOCAL_BUILD_RES_PT3 = $(LOCAL_BUILD_RES)/mus
export LOCAL_BUILD_RES_SFX = $(LOCAL_BUILD_RES)/sfx
export LOCAL_BUILD_OUT_GEN = $(CURDIR)/gen
export LOCAL_BUILD_OUT_ROM = $(LOCAL_BUILD_OUT)/rom
export LOCAL_BUILD_OUT_BIN = $(LOCAL_BUILD_OUT)/bin

$(LOCAL_BUILD_OUT_BIN):
	@mkdir -p $@

$(LOCAL_BUILD_OUT_ROM):
	@mkdir -p $@

$(LOCAL_BUILD_OUT_GEN):
	@mkdir -p $@

clean:
	@rm -Rf $(LOCAL_BUILD_OUT_GEN)
	@rm -Rf $(LOCAL_BUILD_OUT)
