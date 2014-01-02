#include "libtest/testutils.h"
#include "lib/run-id.h"
#include "lib/apphook.h"
#include <unistd.h>

PersistState* create_persist_state(void)
{
  PersistState* state;
  unlink("test_run_id.persist");
  state = persist_state_new("test_run_id.persist");
  persist_state_start(state);
  return state;
};

void destroy_persist_state(PersistState* state)
{
  persist_state_cancel(state);
  persist_state_free(state);
  unlink("test_run_id.persist");
};

void test_run_id__first_run__run_id_is_one(void)
{
  PersistState *state;
  state = create_persist_state();

  run_id_init(state);

  assert_gint(run_id_get(), RUN_ID_FIRST, "Newly initialized run id is not the first id!");

  destroy_persist_state(state);
};

void test_run_id__second_run__run_id_is_two(void)
{
  PersistState *state;

  state = create_persist_state();

  run_id_init(state);
  persist_state_commit(state);
  persist_state_free(state);

  state = persist_state_new("test_run_id.persist");
  persist_state_start(state);
  run_id_init(state);
  assert_gint(run_id_get(), RUN_ID_FIRST + 1, "running run_id_init twice is not the second id!");

  destroy_persist_state(state);
};

void test_run_id__second_run_but_with_non_commit__run_id_is_one(void)
{
  PersistState *state;

  state = create_persist_state();

  run_id_init(state);
  persist_state_cancel(state);
  persist_state_free(state);

  state = persist_state_new("test_run_id.persist");
  persist_state_start(state);
  run_id_init(state);
  assert_gint(run_id_get(), RUN_ID_FIRST, "not committing persist state still increases run_id");

  destroy_persist_state(state);
};

void test_run_id__is_same_run__differs_when_not_same_run(void)
{
  PersistState *state;

  state = create_persist_state();

  run_id_init(state);
  int prev_run_id = run_id_get();
  persist_state_cancel(state);
  persist_state_free(state);

  state = persist_state_new("test_run_id.persist");
  persist_state_start(state);
  run_id_init(state);
  assert_false(run_id_is_same_run(prev_run_id), "run_id_is_same_run returned true when the run differs");

  destroy_persist_state(state);
};

void test_run_id_macro__macro_has_the_same_value_as_run_id(void)
{
  PersistState *state;
  GString* res = g_string_sized_new(0);

  state = create_persist_state();
  run_id_init(state);

  run_id_append_formatted_id(res);
  assert_string(res->str, "1", "Run id cannot be formatted");

  destroy_persist_state(state);
  g_string_free(res, TRUE);
};

void test_run_id_macro__macro_is_empty_if_run_id_is_not_inited(void)
{
  GString* res = g_string_sized_new(0);

  run_id_deinit();

  run_id_append_formatted_id(res);
  assert_string(res->str, "", "Run id is not empty if it is not inited");

  g_string_free(res, TRUE);
};


int main()
{
  app_startup();
  test_run_id__first_run__run_id_is_one();
  test_run_id__second_run__run_id_is_two();
  test_run_id__second_run_but_with_non_commit__run_id_is_one();
  test_run_id__is_same_run__differs_when_not_same_run();
  test_run_id_macro__macro_has_the_same_value_as_run_id();
  test_run_id_macro__macro_is_empty_if_run_id_is_not_inited();
  app_shutdown();
  return 0;
};
