##### what's this?

This contains examples of overriding classes inside the apk that you are launching,
currently for the purposes of avoiding having to make stuff not crash that we don't even
care about. (ideally we would figure out why it crashes and make sure it doesn't do that,
but it's hard to motivate oneself to do that when the issue is with something that has no
place being part of the app to begin with)

##### how to apply the override?

Currently, the way to apply an override is to use the `Makefile` to compile `classes3.dex`,
and then to add this file to the `api-impl.jar`, making it part of bootclasspath.  
If it's enough to have it be part of classpath, then adding support for specifying multiple
classpath entries would also solve this issue; otherwise a way to specify additional
bootclasspath entries would be needed.
