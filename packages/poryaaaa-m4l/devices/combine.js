inlets = 1
outlets = 2

let vgName = "";
let projectPath = "";

function setPath(path) {
    projectPath = path;
    post(projectPath)
}
function setVgName(vName) {
    vgName = vName
    post(vgName)
}
function voicegroup(path, vg) {
    if (path === undefined || vg === undefined) {
        post("Error in codebox, undefined path or vg")
        return;        
    }
    projectPath = path;
    vgName = vg;
}
function bang() {
    if (projectPath === undefined || vgName === undefined) {
        post("Error in codebox, undefined path or vg")
        return;        
    }
        
 }