package com.flycast.emulator;
import org.junit.Test;
import static org.junit.Assert.*;
import java.util.Arrays;
public class Hotd2ArcadeStartStatusTest {
    @Test public void completeCartridgeMarksTwoFilesWithoutChd() {Hotd2ArcadeFiles.Result r=new Hotd2ArcadeFiles.Result();Hotd2ArcadeStartStatus s=Hotd2ArcadeStartStatus.from(r,true);assertTrue(s.ready);assertEquals(Arrays.asList("hotd2.zip ✓","hod2bios.zip ✓"),s.fileMarks);}
    @Test public void missingChipCannotShowReady() {Hotd2ArcadeFiles.Result r=new Hotd2ArcadeFiles.Result();r.missing.add("hotd2.zip: needs mpr-21405.ic20s");Hotd2ArcadeStartStatus s=Hotd2ArcadeStartStatus.from(r,true);assertFalse(s.ready);assertEquals("MISSING FILES",s.badge);assertEquals(1,s.missing.size());assertTrue(s.missing.get(0).contains("mpr-21405.ic20s"));}
    @Test public void missingBiosExplainsLocation() {Hotd2ArcadeFiles.Result r=new Hotd2ArcadeFiles.Result();r.missing.add("hod2bios.zip: beside hotd2.zip or inside it");Hotd2ArcadeStartStatus s=Hotd2ArcadeStartStatus.from(r,true);assertFalse(s.ready);assertTrue(s.missing.get(0).contains("beside hotd2.zip"));}
    @Test public void ownStorageCanBeReadyWithoutSharedAccess(){assertTrue(Hotd2ArcadeStartStatus.from(new Hotd2ArcadeFiles.Result(),false).ready);}
    @Test public void sharedAccessRequestedOnlyWhenFilesIncomplete(){Hotd2ArcadeFiles.Result r=new Hotd2ArcadeFiles.Result();r.missing.add("hotd2.zip: missing");assertEquals("ALLOW ACCESS",Hotd2ArcadeStartStatus.from(r,false).badge);}
    @Test public void initialStatusIsChecking(){assertEquals("CHECKING",Hotd2ArcadeStartStatus.from(null,true).badge);assertEquals("ALLOW ACCESS",Hotd2ArcadeStartStatus.from(null,false).badge);}
    @Test public void ownVersionIsReadable(){assertEquals("v1.0",Hotd2ArcadeStartStatus.shortVersion("1.0.0-gabc123"));assertEquals("",Hotd2ArcadeStartStatus.shortVersion(null));}
}