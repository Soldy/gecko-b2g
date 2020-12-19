#!/usr/bin/env bash
set -e

info() {
  printf "[check-format]\033[1;33m[INFO]${1}\033[0m\n"
}

warn() {
  printf "[check-format]\033[0;31m[WARN]${1}\033[0m\n"
}

help() {
  echo "Usage: $0 [--against <any ref>] [-u|--untracked] [--koost] [-v] [--fix]"
  echo
  echo "Available options:"
  echo "--against <any ref>"
  echo "  any ref you want git to compare with, in order for the set of added, renamed and the modified files."
  echo "  the value passed along with this options can be any acceptable refspec such as commit, branch and tag."
  echo "  by default HEAD if not provided."
  echo
  echo "-u | --untracked"
  echo "  lint the untracked files too."
  echo
  echo "--koost"
  echo "  Lint the changed files in koost."
  echo "  Note that the ref passed along with --against is necessarily reachable in koost when this option is specified."
  echo "   Otherwise \"fatal: bad object\" would be reported by git."
  echo
  echo "-v, --fix"
  echo "  orginal options for \"mach lint\", see \"mach lint --help\" for more details."
  echo
  echo "-h | --help"
  echo "  Show this message."
  echo
}

check_merge_base() {
  # if the ci is triggered by a different repository, need to check in the right clone dir.
  pushd ${GIT_CLONE_PATH}
  local remote_head=`git ls-remote -q --refs ${CI_MERGE_REQUEST_PROJECT_URL}.git ${CI_MERGE_REQUEST_TARGET_BRANCH_NAME} | tr -s ' ' | cut -f 1`
  local merge_base=`git log --pretty=tformat:"%H" | grep ${remote_head}`
  if [ -z "${merge_base}" ]; then
    warn "Please rebase and update the MR."
    exit -1
  fi
  popd
}

check_commit_msg() {
  COMMIT_MESSAGE_PATTERN="^(Bug|MozBug) [[:digit:]]+ \- "
  local msg="${1}"

  if ! [[ ${msg} =~ ${COMMIT_MESSAGE_PATTERN} ]]; then
    warn "Commit message format error. Please correct it by following the format as below:"
    echo "Bug xxxxxxx - [A brief decription] r=frist_name.last_name[, r=r2, r=r3 ...]"
    exit -1
  fi
}

trap 'warn "$(basename $0) caught error on line : $LINENO command was: $BASH_COMMAND"; exit 1' ERR
GIT=git
lint_opts=()
untracked=false
is_koost=false
unset against_ref
while [ $# -ge 1 ]; do
  case $1 in
    --against)
      shift
      against_ref="$1"
      shift
      ;;
    -u|--untracked)
      untracked=true
      shift
      ;;
    --koost)
      shift
      [ -d "${PWD}/koost/.git" ] || { warn "koost not found."; exit -1; }
      GIT="${GIT} --work-tree=${PWD}/koost --git-dir=${PWD}/koost/.git"
      is_koost=true
      ;;
    -v|--fix)
    # known options for `mach lint`
      lint_opts+=("$1")
      shift
      ;;
    -h|--help)
      help
      exit 0
      ;;
    -*)
      warn "Unknown options: $1"
      exit -1
      ;;
  esac
done

against_ref=${against_ref:-HEAD}
all_files=()
js_files=()
other_files=()

info "Diff against ${against_ref}"
if [[ ${against_ref} == HEAD ]]; then
  # assume that all changed files are unstaged
  IFS=$'\n' read -r -d '' -a all_files < <( ${GIT} diff ${against_ref} --diff-filter=ARM --name-only && printf '\0' )
  if [ ${#all_files[@]} -eq 0 ]; then
  # try the staged files
    warn "No changed files compared to ${against_ref} found, I assume you already executed 'git add'. Retry with --cached"
    IFS=$'\n' read -r -d '' -a all_files < <( ${GIT} diff ${against_ref} --diff-filter=ARM --cached --name-only && printf '\0' )
  fi
else
  # check the commit message format for each commit from ${against_ref} to HEAD
  for c in `${GIT} rev-list ${against_ref}.. --reverse --abbrev-commit --abbrev=7`; do
    info "Validating the commit message format of ${c} ..."
    check_commit_msg "$(${GIT} show ${c} -q --format=%s)"
  done
  echo
  # get the ADDED, RENAMED and MODIFIED files compared to ${against_ref}
  info "Get files added / renamed / modified since ${against_ref} (real ID: `${GIT} rev-parse ${against_ref}`)"
  IFS=$'\n' read -r -d '' -a all_files < <( ${GIT} diff ${against_ref} --diff-filter=ARM --name-only && printf '\0' )
fi
if ${untracked}; then
  info "Get untracked files (that are not recorded in .gitignore)"
  for u in `${GIT} ls-files . --exclude-standard --others`; do
    all_files+=(${u})
  done
fi
if [ ${#all_files[@]} -eq 0 ]; then
  warn "No files changed ? If you want to lint all files in gecko, use 'mach lint' directly"
  exit 0
fi

for f in "${all_files[@]}"; do
  if ${is_koost}; then
    f=${f/#/koost\/}
  fi
  echo $f
  ext=${f##*.}
  if [[ $ext =~ ^js[m]? ]]; then
    js_files+=(${f})
  elif [[ $ext =~ ^(c|cpp|cc|css|rs|.*idl)$ ]]; then
    other_files+=(${f})
  fi
done
trap - ERR

if [ ${#js_files[@]} -gt 0 ]; then
  info "Lint js/jsm files with eslint..."
  ./mach lint -f unix "${lint_opts[@]}" -l eslint "${js_files[@]}" || {
    warn "============================================================"
    warn "Please fix linting errors:"
    warn "Run './mach lint --fix -v ${js_files[*]}' "
    warn "for more details."
    warn "============================================================"
    exit 1
  }
fi
if [ ${#other_files[@]} -gt 0 ]; then
  info "Literally Lint the other files..."
  ./mach lint -f unix "${lint_opts[@]}" "${other_files[@]}" || {
    warn "============================================================"
    warn "Please fix linting errors:"
    warn "Run './mach lint --fix -v ${other_files[*]}'"
    warn "for more details."
    warn "============================================================"
    exit 1
  }
fi

# bail out if the base is out of date.
check_merge_base

exit 0
