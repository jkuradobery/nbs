RESOURCES_LIBRARY()

NO_YMAKE_PYTHON3()
SET(RESOURCES_LIBRARY_LINK $TOUCH_UNIT)
DECLARE_EXTERNAL_HOST_RESOURCES_BUNDLE_BY_JSON(YMAKE_PYTHON3 resources.json)

END()
